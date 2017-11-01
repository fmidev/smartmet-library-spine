// ======================================================================
/*!
 * \brief Implementation of class Reactor
 */
// ======================================================================

#include "Reactor.h"
#include "DynamicPlugin.h"
#include "Exception.h"
#include "Names.h"
#include "Options.h"
#include "SmartMet.h"
#include "SmartMetEngine.h"

// sleep?t=n queries only in debug mode
#ifndef NDEBUG
#include "Convenience.h"
#endif

#include <macgyver/StringConversion.h>

#include <mysql.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>
#include <boost/locale.hpp>
#include <boost/make_shared.hpp>

#include <macgyver/AnsiEscapeCodes.h>

#include <dlfcn.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

extern "C" {
#include <unistd.h>
}

namespace fs = boost::filesystem;
namespace ba = boost::algorithm;

namespace
{
void absolutize_path(std::string& file_name)
{
  if (!file_name.empty() && fs::exists(file_name))
  {
    fs::path path(file_name);
    if (!path.is_absolute())
    {
      path = fs::canonical(path);
      file_name = path.string();
    }
  }
}
int is_file_readable(std::string& file_name)
{
  int handle;
  // Open appears to be the only practical solution
  // Use POSIX syscalls here for most efficient solution
  if ((handle = open(file_name.c_str(), O_RDONLY)) >= 0)
  {
    // Open is not enough: directory might be opened but is not readable
    // On NFS: open(2) man page says reading might fail even if open succeeds
    std::size_t nbytes = 1;
    char buf[nbytes];
    auto rdtest = read(handle, static_cast<void*>(buf), nbytes);
    close(handle);
    if (rdtest < 0)  // 0 bytes might be returned, if file has no data
      return errno;
    return 0;
  }
  return errno;
}
}

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Reactor::~Reactor()
{
  // Debug output
  std::cout << "SmartMet Frontend stopping..." << std::endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct from given options
 */
// ----------------------------------------------------------------------

Reactor::Reactor(Options& options)
    : APIVersion(SMARTMET_API_VERSION),
      itsOptions(options),
      itsContentMutex(),
      itsHandlers(),
      itsCatchNoMatch(false),
      itsLoggingMutex(),
      itsLoggingEnabled(false),
      itsLogCleanerThread(),
      itsShutdownRequested(false)
{
  try
  {
    // Startup message
    if (!itsOptions.quiet)
    {
      std::cout << ANSI_ITALIC_ON << "+ SmartMet Server "
                << "(compiled on " __DATE__ " " __TIME__ ")" << std::endl
                << "  Copyright (c) Finnish Meteorological Institute" << ANSI_ITALIC_OFF
                << std::endl
                << std::endl;
    }

    if (itsOptions.verbose)
      itsOptions.report();

    // This has to be done before threading starts
    mysql_library_init(0, NULL, NULL);

    // Initialize the locale before engines are loaded

    boost::locale::generator gen;
    std::locale::global(gen(itsOptions.locale));
    std::cout.imbue(std::locale());

    // Load engines - all or just the requested ones

    const std::string enginedir = itsOptions.directory + "/engines";

    auto& root = itsOptions.itsConfig->get_root();

    if (!root.exists("engines"))
    {
      loadEngines(enginedir, itsOptions.verbose);
    }
    else
    {
      auto& engines = root["engines"];

      if (!engines.isGroup())
        throw SmartMet::Spine::Exception(BCP, "engines-setting must be a group of settings", NULL);

      for (int i = 0; i < engines.getLength(); i++)
      {
        auto& settings = engines[i];

        if (!settings.isGroup())
          throw SmartMet::Spine::Exception(BCP, "engine settings must be groups", NULL);

        if (settings.getName() == NULL)
          throw SmartMet::Spine::Exception(BCP, "engine settings must have names", NULL);

        std::string name = settings.getName();
        std::string defaultfile = enginedir + "/" + name + ".so";
        std::string libfile =
            itsOptions.itsConfig->get_optional_path("engines." + name + ".libfile", defaultfile);

        loadEngine(libfile, itsOptions.verbose);
      }
    }

    // Load plugins

    const std::string plugindir = itsOptions.directory + "/plugins";

    if (!root.exists("plugins"))
    {
      addPlugins(plugindir, itsOptions.verbose);
    }
    else
    {
      auto& plugins = root["plugins"];

      if (!plugins.isGroup())
        throw SmartMet::Spine::Exception(BCP, "plugins-setting must be a group of settings", NULL);

      for (int i = 0; i < plugins.getLength(); i++)
      {
        auto& settings = plugins[i];

        if (!settings.isGroup())
          throw SmartMet::Spine::Exception(BCP, "plugin settings must be groups", NULL);

        if (settings.getName() == NULL)
          throw SmartMet::Spine::Exception(BCP, "plugin settings must have names", NULL);

        std::string name = settings.getName();
        std::string defaultfile = plugindir + "/" + name + ".so";
        std::string libfile =
            itsOptions.itsConfig->get_optional_path("plugins." + name + ".libfile", defaultfile);

        addPlugin(libfile, itsOptions.verbose);
      }
    }

    // Set ContentEngine default logging. Do this after plugins are loaded so handlers are
    // recognized

    setLogging(itsOptions.defaultlogging);

    // List services in verbose mode

    // if(itsOptions.verbose)
    //   {
    // 	listEngines();
    // 	listPlugins();

    // 	std::cout << std::endl << "Registered services:" << std::endl;
    // 	BOOST_FOREACH(const auto & uri_func, itsHandlers)
    // 	  {
    // 		std::cout << "  " << uri_func.first << std::endl;
    // 	  }
    // 	std::cout << itsHandlers.size() << " registered services" << std::endl;

    //   }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Function called by DynamicPlugin to inform that a particular plugin has been initialized
 */
// ----------------------------------------------------------------------

void Reactor::pluginInitializedCallback(DynamicPlugin* plugin)
{
  size_t initialized = 0;
  for (auto& plugin : itsPlugins)
    if (plugin->isInitialized())
      initialized++;

  std::cout << std::string("Plugin ") + plugin->pluginname() + " initialized (" +
                   std::to_string(initialized) + "/" + std::to_string(itsPlugins.size()) + ")\n";

  if (initialized == itsPlugins.size())
    std::cout << std::string("All ") + std::to_string(initialized) + " plugins initialized\n";
}

// ----------------------------------------------------------------------
/*!
 * \brief Required API version
 */
// ----------------------------------------------------------------------

int Reactor::getRequiredAPIVersion() const
{
  return APIVersion;
}
// ----------------------------------------------------------------------
/*!
 * \brief Method to register new URI/CallBackFunction association.
 */
// ----------------------------------------------------------------------

bool Reactor::addContentHandler(SmartMetPlugin* thePlugin,
                                const std::string& theUri,
                                ContentHandler theHandler)
{
  try
  {
    if (itsShutdownRequested)
      return true;

    WriteLock lock(itsContentMutex);
    WriteLock lock2(itsLoggingMutex);
    std::string pluginName = thePlugin->getPluginName();

    auto itsFilter = itsIPFilters.find(Fmi::ascii_tolower_copy(pluginName));
    boost::shared_ptr<IPFilter::IPFilter> theFilter;
    if (itsFilter != itsIPFilters.end())
    {
      theFilter = itsFilter->second;
    }

    boost::shared_ptr<HandlerView> theView(new HandlerView(
        theHandler, theFilter, thePlugin, theUri, itsLoggingEnabled, itsOptions.accesslogdir));

    std::cout << ANSI_BOLD_ON << ANSI_FG_GREEN << "Registered URI " << theUri << " for plugin "
              << thePlugin->getPluginName() << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;

    // Set the handler and filter
    return itsHandlers.insert(Handlers::value_type(theUri, theView)).second;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the handler for unrecognized requests
 */
// ----------------------------------------------------------------------

bool Reactor::setNoMatchHandler(ContentHandler theHandler)
{
  try
  {
    WriteLock lock(itsContentMutex);

    // Catch everything that is specifically not added elsewhere.
    if (theHandler != 0)
    {
      // Set the data members
      boost::shared_ptr<HandlerView> theView(new HandlerView(theHandler));
      itsCatchNoMatchHandler = theView;
      itsCatchNoMatch = true;
    }
    else
    {
      // If function pointer was 0, disable catching hook.
      itsCatchNoMatch = false;
    }

    return itsCatchNoMatch;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Obtain the Handler view corresponding to the given request
 */
// ----------------------------------------------------------------------

boost::optional<HandlerView&> Reactor::getHandlerView(const HTTP::Request& theRequest)
{
  try
  {
    ReadLock lock(itsContentMutex);

    // Try to find a content handler
    auto it = itsHandlers.find(theRequest.getResource());
    if (it == itsHandlers.end())
    {
      // No specific match found, decide what we should do
      if (itsCatchNoMatch == true)
      {
        // Return with true, as this was catched by external handler
        return boost::optional<HandlerView&>(*itsCatchNoMatchHandler);
      }
      else
      {
        // No match found -- return with failure
        return boost::optional<HandlerView&>();
      }
    }

    return boost::optional<HandlerView&>(*(it->second));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Getting logging mode
 */
// ----------------------------------------------------------------------

bool Reactor::getLogging() const
{
  ReadLock lock(itsLoggingMutex);
  return itsLoggingEnabled;
}

// ----------------------------------------------------------------------
/*!
 * \brief Getting logging mode
 */
// ----------------------------------------------------------------------

bool Reactor::lazyLinking() const
{
  return itsOptions.lazylinking;
}
// ----------------------------------------------------------------------
/*!
 * \brief Get copy of the log
 */
// ----------------------------------------------------------------------

Reactor::AccessLogStruct Reactor::getLoggedRequests() const
{
  try
  {
    if (itsLoggingEnabled)
    {
      ReadLock lock(itsContentMutex);
      LoggedRequests theRequests;
      for (auto it = itsHandlers.begin(); it != itsHandlers.end(); ++it)
      {
        theRequests.insert(std::make_pair(it->first, it->second->getLoggedRequests()));
      }
      return std::make_tuple(true, theRequests, itsLogLastCleaned);
    }
    else
    {
      return std::make_tuple(false, LoggedRequests(), boost::posix_time::ptime());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get registered URIs
 */
// ----------------------------------------------------------------------

URIMap Reactor::getURIMap() const
{
  try
  {
    ReadLock lock(itsContentMutex);
    URIMap theMap;

    BOOST_FOREACH (auto& handlerPair, itsHandlers)
    {
      theMap.insert(std::make_pair(handlerPair.first, handlerPair.second->getPluginName()));
    }

    return theMap;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Clean the log of old entries
 */
// ----------------------------------------------------------------------

/* [[noreturn]] */ void Reactor::cleanLog()
{
  try
  {
    // This function must be called as an argument to the cleaner thread

    auto maxAge = boost::posix_time::hours(24);  // Here we give the maximum log time span, 24 hours

    while (!itsShutdownRequested)
    {
      // Sleep for some time
      boost::this_thread::sleep(boost::posix_time::seconds(5));

      auto firstValidTime = boost::posix_time::second_clock::local_time() - maxAge;
      BOOST_FOREACH (auto& handlerPair, itsHandlers)
      {
        if (itsShutdownRequested)
          return;

        handlerPair.second->cleanLog(firstValidTime);
        handlerPair.second->flushLog();
      }

      if (itsLogLastCleaned < firstValidTime)
      {
        itsLogLastCleaned = firstValidTime;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set request logging activity on or off. Only call after the handlers are registered
 */
// ----------------------------------------------------------------------

void Reactor::setLogging(bool loggingEnabled)
{
  try
  {
    WriteLock lock(itsLoggingMutex);
    bool previousStatus = itsLoggingEnabled;  // Save previous status to detect changes
    itsLoggingEnabled = loggingEnabled;

    if (itsLoggingEnabled == previousStatus)
    {
      // No change in status, simply return
      return;
    }

    if (itsLoggingEnabled)
    {
      // See if cleaner thread is running for some reason
      if (itsLogCleanerThread.get() != 0 && itsLogCleanerThread->joinable())
      {
        // Kill any remaining thread
        itsLogCleanerThread->interrupt();
        itsLogCleanerThread->join();
      }

      // Launch log cleaner thread
      itsLogCleanerThread.reset(new boost::thread(boost::bind(&Reactor::cleanLog, this)));

      // Set log cleanup time
      itsLogLastCleaned = boost::posix_time::second_clock::local_time();
    }
    else
    {
      // Status set to false, make the transition true->false
      // Erase log, stop cleaning thread
      itsLogCleanerThread->interrupt();
      itsLogCleanerThread->join();
    }

    // Set logging status for ALL plugins
    BOOST_FOREACH (auto& handlerPair, itsHandlers)
    {
      handlerPair.second->setLogging(itsLoggingEnabled);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief List the names of the loaded plugins
 */
// ----------------------------------------------------------------------

void Reactor::listPlugins() const
{
  try
  {
    // List all plugins

    std::cout << std::endl << "List of plugins:" << std::endl;

    for (auto it = itsPlugins.begin(); it != itsPlugins.end(); it++)
    {
      std::cout << "  " << (*it)->filename() << '\t' << (*it)->pluginname() << '\t'
                << (*it)->apiversion() << std::endl;
    }

    // Number of plugins
    std::cout << itsPlugins.size() << " plugin(s) loaded in memory." << std::endl << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Load a new plugin
 */
// ----------------------------------------------------------------------

bool Reactor::addPlugin(const std::string& theFilename, bool verbose)
{
  try
  {
    std::string pluginname = Names::plugin_name(theFilename);

    bool disabled = itsOptions.itsConfig->get_optional_config_param<bool>(
        "plugins." + pluginname + ".disabled", false);
    if (disabled)
    {
      if (verbose)
      {
        std::cout << ANSI_FG_YELLOW << "\t  + [Ignoring dynamic plugin '" << theFilename
                  << "' since disabled flag is true]" << ANSI_FG_DEFAULT << std::endl;
      }
      return false;
    }

    std::string configfile =
        itsOptions.itsConfig->get_optional_path("plugins." + pluginname + ".configfile", "");
    if (configfile != "")
    {
      absolutize_path(configfile);

      if (is_file_readable(configfile) != 0)
        throw SmartMet::Spine::Exception(BCP,
                                         "plugin " + pluginname + " config " + configfile +
                                             " is unreadable: " + std::strerror(errno));
    }

    // Find the ip filters
    std::vector<std::string> filterTokens;
    itsOptions.itsConfig->get_config_array<std::string>("plugins." + pluginname + ".ip_filters",
                                                        filterTokens);

    if (not filterTokens.empty())
    {
      boost::shared_ptr<IPFilter::IPFilter> theFilter;

      try
      {
        theFilter.reset(new IPFilter::IPFilter(filterTokens));
        std::cout << "IP Filter registered for plugin: " << pluginname << std::endl;
      }
      catch (std::runtime_error& err)
      {
        // No IP filter for this plugin
        std::cout << "No IP filter for plugin: " << pluginname << ". Reason: " << err.what()
                  << std::endl;
      }

      auto inserted = itsIPFilters.insert(std::make_pair(pluginname, theFilter));
      if (!inserted.second)
      {
        // Plugin name is not unique
        std::cout << "No IP filter for plugin: " << pluginname << ". Reason: plugin name not unique"
                  << std::endl;
      }
    }

    boost::shared_ptr<DynamicPlugin> plugin(new DynamicPlugin(theFilename, configfile, *this));

    if (plugin.get() != 0)
    {
      // Start to initialize the plugin

      itsInitThreads.push_back(
          boost::make_shared<boost::thread>(boost::bind(&DynamicPlugin::initializePlugin, plugin)));

      itsPlugins.push_back(plugin);
      return true;
    }
    else
      return false;  // Should we throw??
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Load plugins from a directory
 */
// ----------------------------------------------------------------------

bool Reactor::addPlugins(const std::string& theDirectory, bool verbose)
{
  try
  {
    if (!fs::exists(theDirectory))
      throw SmartMet::Spine::Exception(BCP,
                                       "Plugin directory '" + theDirectory + "' does not exist!");

    if (!fs::is_directory(theDirectory))
      throw SmartMet::Spine::Exception(
          BCP, "Expecting '" + theDirectory + "' to be a plugin directory!");

    if (verbose)
    {
      std::cout << "\t+ [Loading plugins in directory '" << theDirectory << "']" << std::endl;
    }

    unsigned int count = 0;
    fs::directory_iterator end_dir;
    for (fs::directory_iterator it(theDirectory); it != end_dir; ++it)
    {
      if (fs::extension(*it) == ".so")
        if (addPlugin(it->path().string(), verbose))
          ++count;
    }

    return (count > 0);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Method to create a new instance of loaded class
 */
// ----------------------------------------------------------------------

void* Reactor::newInstance(const std::string& theClassName, void* user_data)
{
  try
  {
    // Search for the class creator

    auto it = itsEngines.find(theClassName);
    if (it == itsEngines.end())
    {
      std::cerr << ANSI_FG_RED << "Unable to create a new instance of engine class '"
                << theClassName << "'" << std::endl
                << "No such class was found loaded in the EngineHood" << ANSI_FG_DEFAULT
                << std::endl;
      return 0;
    }

    // config names are all lower case
    std::string name = Fmi::ascii_tolower_copy(theClassName);
    std::string configfile = itsEngineConfigs.find(name)->second;

    // Construct the new engine instance
    void* engineInstance = it->second(configfile.c_str(), user_data);

    SmartMetEngine* theEngine = reinterpret_cast<SmartMetEngine*>(engineInstance);

    // Fire the initialization thread
    itsInitThreads.push_back(boost::make_shared<boost::thread>(
        boost::bind(&SmartMetEngine::construct, theEngine, theClassName)));

    return engineInstance;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Method to create a new instance of loaded class
 */
// ----------------------------------------------------------------------

Reactor::EngineInstance Reactor::getSingleton(const std::string& theClassName,
                                              void* /* user_data */)
{
  try
  {
    if (itsShutdownRequested)
    {
      // Notice that this exception most likely terminates a plugin's initialization
      // phase when the plugin requests an engine. This exception is usually
      // caught in the plugin's initPlugin() method.

      throw SmartMet::Spine::Exception(BCP, "Shutdown active!");
    }

    Reactor::EngineInstance result;

    // Search for the singleton in
    auto it = itsSingletons.find(theClassName);

    if (it == itsSingletons.end())
    {
      // Log error and return with zero
      std::cout << ANSI_FG_RED << "No engine '" << theClassName << "' was found loaded in memory."
                << ANSI_FG_DEFAULT << std::endl;

      return 0;
    }
    else
    {
      // Found it, return the already-created instance of class.
      result = it->second;
    }

    // Engines must be wait() - ed before use, do it here so plugins don't have worry about it

    SmartMetEngine* thisEngine = reinterpret_cast<SmartMetEngine*>(result);

    thisEngine->wait();

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Dynamic Engine loader
 */
// ----------------------------------------------------------------------

bool Reactor::loadEngine(const std::string& theFilename, bool verbose)
{
  try
  {
    // Open the dynamic library specified by private data
    // member module_filename

    std::string enginename = Names::engine_name(theFilename);

    bool disabled = itsOptions.itsConfig->get_optional_config_param<bool>(
        "engines." + enginename + ".disabled", false);
    if (disabled)
    {
      if (verbose)
      {
        std::cout << ANSI_FG_YELLOW << "\t  + [Ignoring engine class '" << theFilename
                  << "' since disabled flag is true]" << ANSI_FG_DEFAULT << std::endl;
      }
      return false;
    }

    std::string configfile =
        itsOptions.itsConfig->get_optional_path("engines." + enginename + ".configfile", "");
    if (configfile != "")
    {
      absolutize_path(configfile);
      if (is_file_readable(configfile) != 0)
        throw SmartMet::Spine::Exception(BCP,
                                         "engine " + enginename + " config " + configfile +
                                             " is unreadable: " + std::strerror(errno));
    }

    itsEngineConfigs.insert(ConfigList::value_type(enginename, configfile));

    // if(verbose)
    //   {
    // 	std::cout << "\t  + [Loading engine class '"
    // 			  << theFilename;
    // 	if(configfile.empty())
    // 	  std::cout << "' with no configfile]";
    // 	else
    // 	  std::cout << "' with configfile '"
    // 				<< configfile
    // 				<< "']";
    // 	std::cout << std::endl;
    //   }

    void* itsHandle = dlopen(theFilename.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (itsHandle == 0)
    {
      // Error occurred while opening the dynamic library
      throw SmartMet::Spine::Exception(
          BCP, "Unable to load dynamic engine class library: " + std::string(dlerror()));
    }

    // Load the symbols (pointers to functions in dynamic library)
    EngineNamePointer itsNamePointer =
        reinterpret_cast<EngineNamePointer>(dlsym(itsHandle, "engine_name"));

    EngineInstanceCreator itsCreatorPointer =
        reinterpret_cast<EngineInstanceCreator>(dlsym(itsHandle, "engine_class_creator"));

    // Check that pointers to function were loaded succesfully
    if (itsNamePointer == 0 || itsCreatorPointer == 0)
    {
      throw SmartMet::Spine::Exception(
          BCP, "Cannot resolve dynamic library symbols: " + std::string(dlerror()));
    }

    // Create a permanent string out of engines human readable name

    if (!itsEngines
             .insert(EngineList::value_type((*itsNamePointer)(),
                                            static_cast<EngineInstanceCreator>(itsCreatorPointer)))
             .second)
      return false;

    // Begin constructing the engine
    auto theSingleton = newInstance(itsNamePointer(), NULL);

    // Check whether the preliminary creation succeeded
    if (theSingleton == 0)
    {
      // Log error and return with zero
      std::cout << ANSI_FG_RED << "No engine '" << itsNamePointer()
                << "' was found loaded in memory." << ANSI_FG_DEFAULT << std::endl;

      return 0;
    }

    itsSingletons.insert(SingletonList::value_type(itsNamePointer(), theSingleton));

    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Dynamic Engine loader
 */
// ----------------------------------------------------------------------

bool Reactor::loadEngines(const std::string& theDirectory, bool verbose)
{
  try
  {
    if (!fs::exists(theDirectory))
      throw SmartMet::Spine::Exception(BCP,
                                       "Engine directory '" + theDirectory + "' does not exist");

    if (!fs::is_directory(theDirectory))
      throw SmartMet::Spine::Exception(
          BCP, "Expecting '" + theDirectory + "' to be an engine directory");

    if (verbose)
    {
      std::cout << "\t+ [Loading engine classes in directory '" << theDirectory << "']"
                << std::endl;
    }

    unsigned int count = 0;
    fs::directory_iterator end_dir;
    for (fs::directory_iterator it(theDirectory); it != end_dir; ++it)
    {
      if (fs::extension(*it) == ".so")
        if (loadEngine(it->path().string(), verbose))
          ++count;
    }

    return (count > 0);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief List the names of the loaded engines
 */
// ----------------------------------------------------------------------

void Reactor::listEngines() const
{
  try
  {
    std::cout << std::endl << "List of engines:" << std::endl;

    for (auto it = itsEngines.begin(); it != itsEngines.end(); it++)
    {
      std::cout << "  " << it->first << std::endl;
    }

    // Number of engines
    std::cout << itsEngines.size() << " engines(s) loaded in memory." << std::endl << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool Reactor::addClientConnectionStartedHook(const std::string& hookName,
                                             ClientConnectionStartedHook theHook)
{
  try
  {
    WriteLock lock(itsHookMutex);
    return itsClientConnectionStartedHooks.insert(std::make_pair(hookName, theHook)).second;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool Reactor::addBackendConnectionFinishedHook(const std::string& hookName,
                                               BackendConnectionFinishedHook theHook)
{
  try
  {
    WriteLock lock(itsHookMutex);
    return itsBackendConnectionFinishedHooks.insert(std::make_pair(hookName, theHook)).second;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool Reactor::addClientConnectionFinishedHook(const std::string& hookName,
                                              ClientConnectionFinishedHook theHook)
{
  try
  {
    WriteLock lock(itsHookMutex);
    return itsClientConnectionFinishedHooks.insert(std::make_pair(hookName, theHook)).second;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Reactor::callClientConnectionStartedHooks(const std::string& theClientIP)
{
  try
  {
    ReadLock lock(itsHookMutex);

    BOOST_FOREACH (auto& pair, itsClientConnectionStartedHooks)
    {
      pair.second(theClientIP);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Reactor::callBackendConnectionFinishedHooks(
    const std::string& theHostName,
    int thePort,
    SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus theStatus)
{
  try
  {
    ReadLock lock(itsHookMutex);

    BOOST_FOREACH (auto& pair, itsBackendConnectionFinishedHooks)
    {
      pair.second(theHostName, thePort, theStatus);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Reactor::callClientConnectionFinishedHooks(const std::string& theClientIP,
                                                const boost::system::error_code& theError)
{
  try
  {
    ReadLock lock(itsHookMutex);
    BOOST_FOREACH (auto& pair, itsClientConnectionFinishedHooks)
    {
      pair.second(theClientIP, theError);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool Reactor::isShutdownRequested()
{
  return itsShutdownRequested;
}

void Reactor::shutdown()
{
  try
  {
    itsShutdownRequested = true;

    // STEP 1: Informing all plugins that the shutdown is in progress. Otherwise
    //         they might start new jobs meanwhile other components are shutting down.

    for (auto it = itsPlugins.begin(); it != itsPlugins.end(); it++)
    {
      (*it)->setShutdownRequestedFlag();
    }

    // STEP 2: Informing all engines that the shutdown is in progress. Otherwise
    //         they might start new jobs meanwhile other components are shutting down.

    for (auto it = itsSingletons.begin(); it != itsSingletons.end(); it++)
    {
      SmartMetEngine* engine = reinterpret_cast<SmartMetEngine*>(it->second);
      engine->setShutdownRequestedFlag();
    }

    // STEP 3: Requesting all plugins to shutdown. Notice that now the plugins know
    // how many requests they have received and how many responses they have sent.
    // In the other words, the plugins wait until the number of responses equals to
    // the number of the requests. This was implemented on the top level, because
    // this way we can relatively safely shutdown all plugins even if the do not
    // implement their own shutdown() method.

    std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nShutdown plugins\n"
              << ANSI_BOLD_OFF << ANSI_FG_DEFAULT;

    for (auto it = itsPlugins.begin(); it != itsPlugins.end(); it++)
    {
      std::cout << ANSI_FG_RED << "* Plugin [" << (*it)->pluginname() << "] shutting down\n"
                << ANSI_FG_DEFAULT;
      (*it)->shutdownPlugin();
      it->reset();
    }

    // STEP 4: Requesting all engines to shutdown.

    std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nShutdown engines\n"
              << ANSI_BOLD_OFF << ANSI_FG_DEFAULT;

    for (auto it = itsSingletons.begin(); it != itsSingletons.end(); it++)
    {
      std::cout << ANSI_FG_RED << "* Engine [" << it->first << "] shutting down\n"
                << ANSI_FG_DEFAULT;
      SmartMetEngine* engine = reinterpret_cast<SmartMetEngine*>(it->second);
      engine->shutdownEngine();
    }

    // STEP 5: Deleting engines. We should not delete engines before they are all shutted down
    //         because they might use other engines (for example, obsengine => geoengine).

    std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nDeleting engines\n"
              << ANSI_BOLD_OFF << ANSI_FG_DEFAULT;

    for (auto it = itsSingletons.begin(); it != itsSingletons.end(); it++)
    {
      std::cout << ANSI_FG_RED << "* Deleting engine [" << it->first << "]\n" << ANSI_FG_DEFAULT;
      SmartMetEngine* engine = reinterpret_cast<SmartMetEngine*>(it->second);
      delete engine;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet
