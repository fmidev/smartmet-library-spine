// ======================================================================
/*!
 * \brief Implementation of class Reactor
 */
// ======================================================================

#include "Reactor.h"
#include "ConfigTools.h"
#include "Convenience.h"
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

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/locale.hpp>
#include <boost/make_shared.hpp>
#include <boost/timer/timer.hpp>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/StringConversion.h>
#include <algorithm>
#include <dlfcn.h>
#include <functional>
#include <iostream>
#include <mysql.h>
#include <stdexcept>
#include <string>

extern "C"
{
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
}  // namespace

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
  std::cout << "SmartMet Server stopping..." << std::endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct from given options
 */
// ----------------------------------------------------------------------

Reactor::Reactor(Options& options) : itsOptions(options)
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

    // Initial limit for simultaneous active requests
    itsActiveRequestsLimit = itsOptions.throttle.start_limit;

    // This has to be done before threading starts
    mysql_library_init(0, nullptr, nullptr);

    // Initialize the locale before engines are loaded

    boost::locale::generator gen;
    std::locale::global(gen(itsOptions.locale));
    std::cout.imbue(std::locale());

    itsInitTasks.on_task_error(
        [this](const std::string& name)
        {
            std::cout << __FILE__ << ":" << __LINE__ << ": init task " << name << " failed" << std::endl;
            // FIXME: stop nicely instead of SIGKILL
            kill(getpid(), SIGKILL);
        });
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Reactor::init()
{
  try {
    // Load engines - all or just the requested ones

    const auto& config = itsOptions.itsConfig;

    if (!config.exists("engines"))
    {
      std::cerr << "Warning: engines setting missing from the server configuration file - are you "
                   "sure you do not need any?"
                << std::endl;
    }
    else
    {
      auto libs = findLibraries("engine");
      itsEngineCount = libs.size();

      for (const auto& libfile : libs)
        loadEngine(libfile, itsOptions.verbose);
    }

    // Load plugins

    if (!config.exists("plugins"))
    {
      throw Spine::Exception(BCP, "plugins setting missing from the server configuration file");
    }
    else
    {
      auto libs = findLibraries("plugin");

      itsPluginCount = libs.size();

      // Then load them in parallel, keeping track of how many have been loaded
      for (const auto& libfile : libs)
        loadPlugin(libfile, itsOptions.verbose);
    }

    try {
        itsInitTasks.wait();
    } catch (...) {
        std::cout << "Initialization failed" << std::endl;
        exit(1);
    }
    // Set ContentEngine default logging. Do this after plugins are loaded so handlers are
    // recognized

    setLogging(itsOptions.defaultlogging);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Find enabled libraries or plugins from the configuration
 */
// ----------------------------------------------------------------------

std::vector<std::string> Reactor::findLibraries(const std::string& theName) const
{
  const auto& name = theName;
  const auto names = theName + "s";

  const auto moduledir = itsOptions.directory + "/" + names;

  const auto& config = itsOptions.itsConfig;
  const auto& modules = config.lookup(names);

  if (!modules.isGroup())
    throw Spine::Exception(BCP, names + "-setting must be a group of settings");

  // Collect all enabled modules

  std::vector<std::string> libs;

  for (int i = 0; i < modules.getLength(); i++)
  {
    auto& settings = modules[i];

    if (!settings.isGroup())
      throw Spine::Exception(BCP, name + " settings must be groups");

    if (settings.getName() == nullptr)
      throw Spine::Exception(BCP, name + " settings must have names");

    std::string module_name = settings.getName();
    std::string libfile = moduledir + "/" + module_name + ".so";
    lookupPathSetting(config, libfile, names + "." + module_name + ".libfile");

    bool disabled = false;
    lookupHostSetting(itsOptions.itsConfig, disabled, names + "." + module_name + ".disabled");

    if (!disabled)
      libs.push_back(libfile);
    else if (itsOptions.verbose)
    {
      std::cout << Spine::log_time_str() << ANSI_FG_YELLOW << "\t  + [Ignoring " << name << " '"
                << module_name << "' since disabled flag is true]" << ANSI_FG_DEFAULT << std::endl;
    }
  }
  return libs;
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
 * \brief Method to register new public URI/CallBackFunction association.
 *
 * Handler added using this method is visible through getURIMap() method
 */
// ----------------------------------------------------------------------

bool Reactor::addContentHandler(SmartMetPlugin* thePlugin,
                                const std::string& theDir,
                                ContentHandler theCallBackFunction)
{
  return addContentHandlerImpl(false, thePlugin, theDir, theCallBackFunction);
}

// ----------------------------------------------------------------------
/*!
 * \brief Method to register new private URI/CallBackFunction association.
 *
 * Handler added using this method is not visible through getURIMap() method
 */
// ----------------------------------------------------------------------

bool Reactor::addPrivateContentHandler(SmartMetPlugin* thePlugin,
                                       const std::string& theDir,
                                       ContentHandler theCallBackFunction)
{
  return addContentHandlerImpl(true, thePlugin, theDir, theCallBackFunction);
}

// ----------------------------------------------------------------------
/*!
 * \brief Implementation of registeration of new private URI/CallBackFunction association.
 *
 */
// ----------------------------------------------------------------------

bool Reactor::addContentHandlerImpl(bool itsPrivate,
                                    SmartMetPlugin* thePlugin,
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
    boost::shared_ptr<IPFilter::IPFilter> filter;
    if (itsFilter != itsIPFilters.end())
    {
      filter = itsFilter->second;
    }

    boost::shared_ptr<HandlerView> theView(new HandlerView(theHandler,
                                                           filter,
                                                           thePlugin,
                                                           theUri,
                                                           itsLoggingEnabled,
                                                           itsPrivate,
                                                           itsOptions.accesslogdir));

    std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN << " Registered "
              << (itsPrivate ? "private " : "") << "URI " << theUri << " for plugin "
              << thePlugin->getPluginName() << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;

    // Set the handler and filter
    return itsHandlers.insert(Handlers::value_type(theUri, theView)).second;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!").addParameter("URI", theUri);
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Removes all handlers that use specified plugin.
 */
// ----------------------------------------------------------------------
std::size_t Reactor::removeContentHandlers(SmartMetPlugin* thePlugin)
{
  std::size_t count = 0;
  WriteLock lock(itsContentMutex);
  for (auto it = itsHandlers.begin(); it != itsHandlers.end();)
  {
    auto curr = it++;
    if (curr->second and curr->second->usesPlugin(thePlugin))
    {
      const std::string uri = curr->second->getResource();
      const std::string name = curr->second->getPluginName();
      itsHandlers.erase(curr);
      count++;
      WriteLock lock2(itsLoggingMutex);
      std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN << " Removed URI " << uri
                << " handled by plugin " << name << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
    }
  }
  return count;
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Getting logging mode
 */
// ----------------------------------------------------------------------

bool Reactor::getLogging() const
{
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

AccessLogStruct Reactor::getLoggedRequests() const
{
  try
  {
    if (itsLoggingEnabled)
    {
      LoggedRequests requests;
      ReadLock lock(itsContentMutex);
      for (auto it = itsHandlers.begin(); it != itsHandlers.end(); ++it)
      {
        requests.insert(std::make_pair(it->first, it->second->getLoggedRequests()));
      }
      return std::make_tuple(true, requests, itsLogLastCleaned);
    }
    else
    {
      return std::make_tuple(false, LoggedRequests(), boost::posix_time::ptime());
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the load is high
 */
// ----------------------------------------------------------------------

bool Reactor::isLoadHigh() const
{
  return itsHighLoadFlag;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get a copy of active requests
 */
// ----------------------------------------------------------------------

ActiveRequests::Requests Reactor::getActiveRequests() const
{
  return itsActiveRequests.requests();
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a new active request
 */
// ----------------------------------------------------------------------

std::size_t Reactor::insertActiveRequest(const HTTP::Request& theRequest)
{
  auto key = itsActiveRequests.insert(theRequest);

  auto n = itsActiveRequests.size();

  if (n < itsActiveRequestsLimit)
  {
    itsHighLoadFlag = false;
    return key;
  }

  // Load is now high

  itsActiveRequestsCounter = 0;

  itsHighLoadFlag = true;

  if (itsOptions.verbose)
    std::cerr << Spine::log_time_str() << " " << itsActiveRequests.size()
              << " active requests, limit is " << itsActiveRequestsLimit << "/"
              << itsOptions.throttle.limit << std::endl;

  // Reduce the limit back down unless already smaller due to being just started
  if (itsActiveRequestsLimit > itsOptions.throttle.restart_limit)
  {
    itsActiveRequestsLimit = itsOptions.throttle.restart_limit;
    if (itsOptions.verbose)
      std::cerr << Spine::log_time_str() << " dropping active requests limit to "
                << itsOptions.throttle.restart_limit << "/" << itsOptions.throttle.limit
                << std::endl;
  }
  return key;
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove an old active request
 */
// ----------------------------------------------------------------------

void Reactor::removeActiveRequest(std::size_t theKey, HTTP::Status theStatusCode)
{
  itsActiveRequests.remove(theKey);

  if (itsActiveRequests.size() < itsActiveRequestsLimit)
    itsHighLoadFlag = false;

  // Update current limit for simultaneous requests only if the request was a success
  //
  // We also ignore the 204 No content response, which the backend uses to indicate
  // the etagged result is still valid, since generating no content successfully
  // does not mean the server is not having load problems for example due to i/o issues.

  if (theStatusCode != HTTP::ok)
    return;

  if (++itsActiveRequestsCounter % itsOptions.throttle.increase_rate == 0)
  {
    if (itsActiveRequestsLimit < itsOptions.throttle.limit)
    {
      auto new_limit = ++itsActiveRequestsLimit;

      if (itsOptions.verbose)
        std::cerr << Spine::log_time_str() << " increased active requests limit to " << new_limit
                  << "/" << itsOptions.throttle.limit << std::endl;
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Start a new active request to a backend
 */
// ----------------------------------------------------------------------

void Reactor::startBackendRequest(const std::string& theHost, int thePort)
{
  itsActiveBackends.start(theHost, thePort);
}

// ----------------------------------------------------------------------
/*!
 * \brief Stop a request to a backend
 */
// ----------------------------------------------------------------------

void Reactor::stopBackendRequest(const std::string& theHost, int thePort)
{
  itsActiveBackends.stop(theHost, thePort);
}

// ----------------------------------------------------------------------
/*!
 * \brief Reset request count to a backend
 */
// ----------------------------------------------------------------------

void Reactor::resetBackendRequest(const std::string& theHost, int thePort)
{
  itsActiveBackends.reset(theHost, thePort);
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the counts for active backend requests
 */
// ----------------------------------------------------------------------

ActiveBackends::Status Reactor::getBackendRequestStatus() const
{
  return itsActiveBackends.status();
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

    for (auto& handlerPair : itsHandlers)
    {
      // Getting plugin names during shutdown may throw due to a call to a pure virtual method.
      // This mitigates the problem, but does not solve it. The shutdown flag should be
      // locked for the duration of this loop.
      if (itsShutdownRequested)
        return {};

      if (not handlerPair.second->isPrivate())
      {
        theMap.insert(std::make_pair(handlerPair.first, handlerPair.second->getPluginName()));
      }
    }

    return theMap;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      for (auto& handlerPair : itsHandlers)
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
    throw Spine::Exception::Trace(BCP, "cleanLog operation failed!");
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

    // Check for no change in status
    if (itsLoggingEnabled == loggingEnabled)
      return;

    itsLoggingEnabled = loggingEnabled;

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
    for (auto& handlerPair : itsHandlers)
    {
      handlerPair.second->setLogging(itsLoggingEnabled);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Load a new plugin
 */
// ----------------------------------------------------------------------

bool Reactor::loadPlugin(const std::string& theFilename, bool verbose)
{
  try
  {
    std::string pluginname = Names::plugin_name(theFilename);

    std::string configfile;
    lookupConfigSetting(itsOptions.itsConfig, configfile, "plugins." + pluginname);

    if (!configfile.empty())
    {
      absolutize_path(configfile);

      if (is_file_readable(configfile) != 0)
        throw Spine::Exception(BCP,
                               "plugin " + pluginname + " config " + configfile +
                                   " is unreadable: " + std::strerror(errno));
    }

    // Find the ip filters
    std::vector<std::string> filterTokens;
    lookupHostStringSettings(
        itsOptions.itsConfig, filterTokens, "plugins." + pluginname + ".ip_filters");

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

      itsInitTasks.add("Load plugin[" + theFilename + "]",
         [this, plugin, pluginname] ()
         {
             initializePlugin(plugin.get(), pluginname);
         });

      itsPlugins.push_back(plugin);
      return true;
    }
    else
      return false;  // Should we throw??
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!").addParameter("Filename", theFilename);
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
    itsInitTasks.add("New engine instance[" + theClassName + "]",
        [this, theEngine, theClassName] () { initializeEngine(theEngine, theClassName); });

    return engineInstance;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!").addParameter("Class", theClassName);
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

      throw Spine::Exception(BCP, "Shutdown active!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!").addParameter("ClassName", theClassName);
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

    std::string configfile;
    lookupConfigSetting(itsOptions.itsConfig, configfile, "engines." + enginename);

    if (configfile != "")
    {
      absolutize_path(configfile);
      if (is_file_readable(configfile) != 0)
        throw Spine::Exception(BCP,
                               "engine " + enginename + " config " + configfile +
                                   " is unreadable: " + std::strerror(errno));
    }

    itsEngineConfigs.insert(ConfigList::value_type(enginename, configfile));

    void* itsHandle = dlopen(theFilename.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (itsHandle == 0)
    {
      // Error occurred while opening the dynamic library
      throw Spine::Exception(
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
      throw Spine::Exception(BCP,
                             "Cannot resolve dynamic library symbols: " + std::string(dlerror()));
    }

    // Create a permanent string out of engines human readable name

    if (!itsEngines
             .insert(EngineList::value_type((*itsNamePointer)(),
                                            static_cast<EngineInstanceCreator>(itsCreatorPointer)))
             .second)
      return false;

    // Begin constructing the engine
    auto theSingleton = newInstance(itsNamePointer(), nullptr);

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
    throw Spine::Exception::Trace(BCP, "Operation failed!").addParameter("Filename", theFilename);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize an engine
 */
// ----------------------------------------------------------------------

void Reactor::initializeEngine(SmartMetEngine* theEngine, const std::string& theName)
{
  boost::timer::cpu_timer timer;
  theEngine->construct(theName, this);
  timer.stop();

  auto now_initialized = itsInitializedEngineCount.fetch_add(1) + 1;

  std::string report =
      (std::string(ANSI_FG_GREEN) + "Engine [" + theName +
       "] initialized in %t sec CPU, %w sec real (" + std::to_string(now_initialized) + "/" +
       std::to_string(itsEngineCount) + ")" + ANSI_FG_DEFAULT);

  std::cout << Spine::log_time_str() << " " << timer.format(2, report) << std::endl;

  if (now_initialized == itsEngineCount)
    std::cout << log_time_str()
              << std::string(ANSI_FG_GREEN) + std::string(" *** All ") +
                     std::to_string(itsEngineCount) + " engines initialized" + ANSI_FG_DEFAULT
              << std::endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize a plugin
 */
// ----------------------------------------------------------------------

void Reactor::initializePlugin(DynamicPlugin* thePlugin, const std::string& theName)
{
  boost::timer::cpu_timer timer;
  thePlugin->initializePlugin();
  timer.stop();

  auto now_initialized = itsInitializedPluginCount.fetch_add(1) + 1;

  std::string report =
      (std::string(ANSI_FG_GREEN) + "Plugin [" + theName +
       "] initialized in %t sec CPU, %w sec real (" + std::to_string(now_initialized) + "/" +
       std::to_string(itsPluginCount) + ")" + ANSI_FG_DEFAULT);

  std::cout << Spine::log_time_str() << " " << timer.format(2, report) << std::endl;

  if (now_initialized == itsPluginCount)
    std::cout << log_time_str()
              << std::string(ANSI_FG_GREEN) + std::string(" *** All ") +
                     std::to_string(itsPluginCount) + " plugins initialized" + ANSI_FG_DEFAULT
              << std::endl;
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Reactor::callClientConnectionStartedHooks(const std::string& theClientIP)
{
  try
  {
    ReadLock lock(itsHookMutex);

    for (auto& pair : itsClientConnectionStartedHooks)
    {
      pair.second(theClientIP);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Reactor::callBackendConnectionFinishedHooks(
    const std::string& theHostName,
    int thePort,
    Spine::HTTP::ContentStreamer::StreamerStatus theStatus)
{
  try
  {
    ReadLock lock(itsHookMutex);

    for (auto& pair : itsBackendConnectionFinishedHooks)
    {
      pair.second(theHostName, thePort, theStatus);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Reactor::callClientConnectionFinishedHooks(const std::string& theClientIP,
                                                const boost::system::error_code& theError)
{
  try
  {
    ReadLock lock(itsHookMutex);
    for (auto& pair : itsClientConnectionFinishedHooks)
    {
      pair.second(theClientIP, theError);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Shutdown operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
