// ======================================================================
/*!
 * \brief Implementation of class Reactor
 */
// ======================================================================

#include "Reactor.h"
#include "ConfigTools.h"
#include "Convenience.h"
#include "DynamicPlugin.h"
#include "FmiApiKey.h"
#include "Names.h"
#include "Options.h"
#include "SmartMet.h"
#include "SmartMetEngine.h"

// sleep?t=n queries only in debug mode
#ifndef NDEBUG
#include "Convenience.h"
#endif

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/bind/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/locale.hpp>
#include <boost/make_shared.hpp>
#include <boost/process/child.hpp>
#include <boost/timer/timer.hpp>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
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

using namespace boost::placeholders;
namespace fs = boost::filesystem;

namespace
{
std::atomic_bool gIsShuttingDown{false};

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
    const std::size_t nbytes = 1;
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

  // Manual cleanup
  itsInitTasks->stop();
  itsInitTasks.reset();
  itsHandlers.clear();
  itsPlugins.clear();
  itsEngines.clear();
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct from given options
 */
// ----------------------------------------------------------------------

Reactor::Reactor(Options& options) : itsOptions(options), itsInitTasks(new Fmi::AsyncTaskGroup)
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

    itsInitTasks->stop_on_error(true);

    itsInitTasks->on_task_error(
        [](const std::string& name)
        {
          if (!isShuttingDown())
          {
            Fmi::Exception::Trace(BCP, "Operation failed").printError();
            std::cout << __FILE__ << ":" << __LINE__ << ": init task " << name << " failed"
                      << std::endl;
          }
        });
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Reactor::init()
{
  try
  {
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
      throw Fmi::Exception(BCP, "plugins setting missing from the server configuration file");
    }

    auto libs = findLibraries("plugin");

    itsPluginCount = libs.size();

    // Then load them in parallel, keeping track of how many have been loaded
    for (const auto& libfile : libs)
      loadPlugin(libfile, itsOptions.verbose);

    try
    {
      itsInitTasks->wait();
    }
    catch (...)
    {
      std::cout << "Initialization failed" << std::endl;
      exit(1);  // NOLINT not thread safe
    }
    // Set ContentEngine default logging. Do this after plugins are loaded so handlers are
    // recognized

    setLogging(itsOptions.defaultlogging);

    itsInitializing = false;
  }
  catch (...)
  {
    // Inform engines and plugings polling the status that we're going down
    gIsShuttingDown = true;
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception(BCP, names + "-setting must be a group of settings");

  // Collect all enabled modules

  std::vector<std::string> libs;

  for (int i = 0; i < modules.getLength(); i++)
  {
    auto& settings = modules[i];

    if (!settings.isGroup())
      throw Fmi::Exception(BCP, name + " settings must be groups");

    if (settings.getName() == nullptr)
      throw Fmi::Exception(BCP, name + " settings must have names");

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
                                ContentHandler theCallBackFunction,
                                bool handlesUriPrefix)
{
  return addContentHandlerImpl(
      false,
      thePlugin,
      theDir,
      theCallBackFunction,
      handlesUriPrefix);
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
                                       ContentHandler theCallBackFunction,
                                       bool handlesUriPrefix)
{
  return addContentHandlerImpl(
      true,
      thePlugin,
      theDir,
      theCallBackFunction,
      handlesUriPrefix);
}

// ----------------------------------------------------------------------
/*!
 * \brief Implementation of registeration of new private URI/CallBackFunction association.
 *
 */
// ----------------------------------------------------------------------

bool Reactor::addContentHandlerImpl(bool isPrivate,
                                    SmartMetPlugin* thePlugin,
                                    const std::string& theUri,
                                    ContentHandler theHandler,
                                    bool handlesUriPrefix)
{
  try
  {
    if (isShuttingDown())
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

    HandlerPtr theView(new HandlerView(theHandler,
                                       filter,
                                       thePlugin,
                                       theUri,
                                       itsLoggingEnabled,
                                       isPrivate,
                                       itsOptions.accesslogdir));

    std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN << " Registered "
              << (isPrivate ? "private " : "") << "URI " << theUri << " for plugin "
              << thePlugin->getPluginName() << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;

    // Set the handler and filter
    bool inserted = itsHandlers.insert(Handlers::value_type(theUri, theView)).second;
    if (inserted && handlesUriPrefix)
    {
      if (!uriPrefixes.insert(theUri).second)
      {
        throw Fmi::Exception(BCP, "Failed to insert URI remap handler for " + theUri);
      }
    }
    return inserted;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("URI", theUri);
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
    if (theHandler != nullptr)
    {
      // Set the data members
      HandlerPtr theView(new HandlerView(theHandler));
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
      uriPrefixes.erase(uri);
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

    bool remapped = false;
    std::string resource = theRequest.getResource();

    for (const auto& item : uriPrefixes)
    {
      std::size_t len = item.length();
      if (resource.substr(0, len) == item)
      {
        remapped = true;
        resource = item;
        break;
      }
    }

    // Try to find a content handler
    auto it = itsHandlers.find(resource);
    if (it == itsHandlers.end())
    {
      if (remapped)
      {
        // Should never happen
        throw Fmi::Exception(BCP, "[INTERNAL ERROR] URI remapping defined, but"
          " handler not found for " + resource);
      }
      // No specific match found, decide what we should do
      if (itsCatchNoMatch)
      {
        // Return with true, as this was catched by external handler
        return boost::optional<HandlerView&>(*itsCatchNoMatchHandler);
      }

      // No match found -- return with failure
      return boost::optional<HandlerView&>();
    }

    return boost::optional<HandlerView&>(*(it->second));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

AccessLogStruct Reactor::getLoggedRequests(const std::string& thePlugin) const
{
  try
  {
    if (itsLoggingEnabled)
    {
      std::string pluginNameInLowerCase = Fmi::ascii_tolower_copy(thePlugin);
      LoggedRequests requests;
      ReadLock lock(itsContentMutex);
      for (const auto& handler : itsHandlers)
      {
        if (pluginNameInLowerCase == "all" ||
            pluginNameInLowerCase == Fmi::ascii_tolower_copy(handler.second->getPluginName()))
          requests.insert(std::make_pair(handler.first, handler.second->getLoggedRequests()));
      }
      return std::make_tuple(true, requests, itsLogLastCleaned);
    }

    return std::make_tuple(false, LoggedRequests(), boost::posix_time::ptime());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
 * \brief Print the given request list
 */
// ----------------------------------------------------------------------

void print_requests(const Spine::ActiveRequests::Requests& requests)
{
  // Based on Admin::Plugin::requestActiveRequests

  auto now = boost::posix_time::microsec_clock::universal_time();

  std::cout << "Printing active requests due to high load:\n"
            << "Number\tID\tTime\tDuration\tIP\tAPIKEY\t\tURI\n";

  std::size_t row = 0;
  for (const auto& id_info : requests)
  {
    const auto id = id_info.first;
    const auto& time = id_info.second.time;
    const auto& req = id_info.second.request;

    auto duration = now - time;

    const bool check_access_token = false;
    auto apikey = FmiApiKey::getFmiApiKey(req, check_access_token);

    // clang-format off
    std::cout << row++ << "\t"
              << Fmi::to_string(id) << "\t"
              << Fmi::to_iso_extended_string(time.time_of_day()) << "\t"
              << Fmi::to_string(duration.total_milliseconds() / 1000.0) << "\t"
              << req.getClientIP() << "\t"
              << (apikey ? *apikey : "-\t") << "\t"
              << req.getURI() << "\n";
    // clang-format on
  }
  std::cout << std::flush;
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

  // Run alert script if needed

  if (n >= itsOptions.throttle.alert_limit && !itsOptions.throttle.alert_script.empty())
  {
    if (itsActiveRequestsLimit < itsOptions.throttle.limit)
    {
      // Do nothing when ramping up
    }
    else if (itsRunningAlertScript)
    {
      // This turns out to be a bit too verbose
      // if (itsOptions.verbose)
      // std::cerr << Spine::log_time_str() << " Alert script already running" << std::endl;
    }
    else
    {
      itsRunningAlertScript = true;

      // First print the active requests since the alert script may be slow
      if (itsOptions.verbose)
        print_requests(getActiveRequests());

      if (!boost::filesystem::exists(itsOptions.throttle.alert_script))
      {
        std::cerr << Spine::log_time_str() << " Configured alert script  "
                  << itsOptions.throttle.alert_script << " does not exist" << std::endl;
      }
      else
      {
        // Run the alert script in a separate thread not to delay the user response too much
        if (itsOptions.verbose)
          std::cerr << Spine::log_time_str() << " Running alert script "
                    << itsOptions.throttle.alert_script << std::endl;

        std::thread thr(
            [this]
            {
              try
              {
                boost::process::child cld(itsOptions.throttle.alert_script);
                cld.wait();
              }
              catch (...)
              {
                std::cerr << Spine::log_time_str() << " Running alert script "
                          << itsOptions.throttle.alert_script << " failed!" << std::endl;
              }
              itsRunningAlertScript = false;
            });
        thr.detach();
      }
    }
  }

  // Check if we should report high load

  if (n < itsActiveRequestsLimit)
  {
    itsHighLoadFlag = false;
    return key;
  }

  // Load is now high

  itsActiveRequestsCounter = 0;  // now new finished active requests yet

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

    for (const auto& handlerPair : itsHandlers)
    {
      // Getting plugin names during shutdown may throw due to a call to a pure virtual method.
      // This mitigates the problem, but does not solve it. The shutdown flag should be
      // locked for the duration of this loop.
      if (isShuttingDown())
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    while (!isShuttingDown())
    {
      // Sleep for some time
      boost::this_thread::sleep(boost::posix_time::seconds(5));

      auto firstValidTime = boost::posix_time::second_clock::local_time() - maxAge;
      for (auto& handlerPair : itsHandlers)
      {
        if (isShuttingDown())
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
    throw Fmi::Exception::Trace(BCP, "cleanLog operation failed!");
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
      if (itsLogCleanerThread.get() != nullptr && itsLogCleanerThread->joinable())
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    for (const auto& plugin : itsPlugins)
    {
      std::cout << "  " << plugin->filename() << '\t' << plugin->pluginname() << '\t'
                << plugin->apiversion() << std::endl;
    }

    // Number of plugins
    std::cout << itsPlugins.size() << " plugin(s) loaded in memory." << std::endl << std::endl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Load a new plugin
 */
// ----------------------------------------------------------------------

bool Reactor::loadPlugin(const std::string& theFilename, bool /* verbose */)
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
        throw Fmi::Exception(BCP,
                             "plugin " + pluginname + " config " + configfile + " is unreadable: " +
                                 std::strerror(errno));  // NOLINT not thread safe
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

    if (plugin.get() != nullptr)
    {
      // Start to initialize the plugin

      itsInitTasks->add("Load plugin[" + theFilename + "]",
                        [this, plugin, pluginname]()
                        { initializePlugin(plugin.get(), pluginname); });

      itsPlugins.push_back(plugin);
      return true;
    }

    return false;  // Should we throw??
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("Filename", theFilename);
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
      return nullptr;
    }

    // config names are all lower case
    std::string name = Fmi::ascii_tolower_copy(theClassName);
    std::string configfile = itsEngineConfigs.find(name)->second;

    // Construct the new engine instance
    void* engineInstance = it->second(configfile.c_str(), user_data);

    auto* theEngine = reinterpret_cast<SmartMetEngine*>(engineInstance);

    // Fire the initialization thread
    itsInitTasks->add("New engine instance[" + theClassName + "]",
                      [this, theEngine, theClassName]()
                      { initializeEngine(theEngine, theClassName); });

    return engineInstance;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("Class", theClassName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Method to create a new instance of loaded class
 */
// ----------------------------------------------------------------------

SmartMetEngine* Reactor::getSingleton(const std::string& theClassName, void* /* user_data */)
{
  try
  {
    if (isShuttingDown())
    {
      // Notice that this exception most likely terminates a plugin's initialization
      // phase when the plugin requests an engine. This exception is usually
      // caught in the plugin's initPlugin() method.

      throw Fmi::Exception(BCP, "Shutdown active!").disableStackTrace();
    }

    // Search for the singleton in
    auto it = itsSingletons.find(theClassName);

    if (it == itsSingletons.end())
    {
      // Log error and return with zero
      std::cout << ANSI_FG_RED << "No engine '" << theClassName << "' was found loaded in memory."
                << ANSI_FG_DEFAULT << std::endl;

      return nullptr;
    }

    // Found it, return the already-created instance of class.
    auto* engine = it->second;

    // Engines must be wait() - ed before use, do it here so plugins don't have worry about it

    engine->wait();

    return isShuttingDown() ? nullptr : engine;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("ClassName", theClassName);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Dynamic Engine loader
 */
// ----------------------------------------------------------------------

bool Reactor::loadEngine(const std::string& theFilename, bool /* verbose */)
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
        throw Fmi::Exception(BCP,
                             "engine " + enginename + " config " + configfile + " is unreadable: " +
                                 std::strerror(errno));  // NOLINT not thread safe
    }

    itsEngineConfigs.insert(ConfigList::value_type(enginename, configfile));

    void* itsHandle = dlopen(theFilename.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (itsHandle == nullptr)
    {
      // Error occurred while opening the dynamic library
      throw Fmi::Exception(BCP,
                           "Unable to load dynamic engine class library: " +
                               std::string(dlerror()));  // NOLINT not thread safe
    }

    // Load the symbols (pointers to functions in dynamic library)

    auto itsNamePointer = reinterpret_cast<EngineNamePointer>(dlsym(itsHandle, "engine_name"));

    auto itsCreatorPointer =
        reinterpret_cast<EngineInstanceCreator>(dlsym(itsHandle, "engine_class_creator"));

    // Check that pointers to function were loaded succesfully
    if (itsNamePointer == nullptr || itsCreatorPointer == nullptr)
    {
      throw Fmi::Exception(BCP,
                           "Cannot resolve dynamic library symbols: " +
                               std::string(dlerror()));  // NOLINT not thread safe
    }

    // Create a permanent string out of engines human readable name

    if (!itsEngines
             .insert(EngineList::value_type((*itsNamePointer)(),
                                            static_cast<EngineInstanceCreator>(itsCreatorPointer)))
             .second)
      return false;

    // Begin constructing the engine
    auto* singleton = newInstance(itsNamePointer(), nullptr);

    // Check whether the preliminary creation succeeded
    if (singleton == nullptr)
    {
      // Log error and return with zero
      std::cout << ANSI_FG_RED << "No engine '" << itsNamePointer()
                << "' was found loaded in memory." << ANSI_FG_DEFAULT << std::endl;

      return false;
    }

    auto* engine = reinterpret_cast<SmartMetEngine*>(singleton);
    itsSingletons.insert(SingletonList::value_type(itsNamePointer(), engine));

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("Filename", theFilename);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize an engine
 */
// ----------------------------------------------------------------------

void Reactor::initializeEngine(SmartMetEngine* theEngine, const std::string& theName)
{
  try
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
  catch (...)
  {
    auto ex = Fmi::Exception::Trace(BCP, "Engine initialization failed!");
    ex.addParameter("Engine", theName);
    ex.force_stack_trace = true;
    std::cerr << ex.getStackTrace() << std::flush;
    throw ex;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize a plugin
 */
// ----------------------------------------------------------------------

void Reactor::initializePlugin(DynamicPlugin* thePlugin, const std::string& theName)
{
  try
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
  catch (...)
  {
    auto ex = Fmi::Exception::Trace(BCP, "Plugin initialization failed!");
    ex.addParameter("Plugin", theName);
    ex.force_stack_trace = true;
    std::cerr << ex.getStackTrace() << std::flush;
    throw ex;
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

    for (const auto& engine : itsEngines)
    {
      std::cout << "  " << engine.first << std::endl;
    }

    // Number of engines
    std::cout << itsEngines.size() << " engines(s) loaded in memory." << std::endl << std::endl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    // This is called by the Connection destructor, hence we must not throw
    Fmi::Exception ex(BCP, "Connections destructing, not calling hooks anymore", nullptr);
    ex.printError();
    // Fall through to the destructor
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

SmartMetEngine* Reactor::getEnginePtr(const std::string& theClassName, void* user_data)
{
  auto* ptr = getSingleton(theClassName, user_data);
  if (ptr != nullptr)
    return ptr;

  if (isShuttingDown())
  {
    throw Fmi::Exception::Trace(
        BCP, "Shutdown in progress - engine " + theClassName + " is not available")
        .disableStackTrace();
  }

  throw Fmi::Exception::Trace(BCP, "No " + theClassName + " engine available");
}

bool Reactor::isShuttingDown()
{
  return gIsShuttingDown;
}

bool Reactor::isInitializing() const
{
  return itsInitializing;
}

void Reactor::shutdown()
{
  try
  {
    // We are no more interested about init task errors when shutdown has been requested
    itsInitTasks->stop_on_error(false);

    gIsShuttingDown = true;

    Fmi::AsyncTaskGroup shutdownTasks;

    // Requesting all plugins to shutdown. Notice that now the plugins know
    // how many requests they have received and how many responses they have sent.
    // In the other words, the plugins wait until the number of responses equals to
    // the number of the requests. This was implemented on the top level, because
    // this way we can relatively safely shutdown all plugins even if the do not
    // implement their own shutdown() method.

    std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nShutdown plugins" << ANSI_BOLD_OFF
              << ANSI_FG_DEFAULT << std::endl;

    for (auto& plugin : itsPlugins)
    {
      std::cout << ANSI_FG_RED << "* Plugin [" << plugin->pluginname() << "] shutting down\n"
                << ANSI_FG_DEFAULT;
      shutdownTasks.add("Plugin [" + plugin->pluginname() + "] shutdown",
                        [&plugin]()
                        {
                          plugin->shutdownPlugin();
                          plugin.reset();
                        });
    }

    shutdownTasks.wait();
    std::cout << ANSI_FG_RED << "* Plugin shutdown completed" << ANSI_FG_DEFAULT << std::endl;

    // STEP 4: Requesting all engines to shutdown.

    std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nShutdown engines" << ANSI_BOLD_OFF
              << ANSI_FG_DEFAULT << std::endl;

    for (auto& singleton : itsSingletons)
    {
      std::ostringstream tmp1;
      tmp1 << ANSI_FG_RED << "* Engine [" << singleton.first << "] shutting down" << ANSI_FG_DEFAULT
           << '\n';
      std::cout << tmp1.str() << std::flush;
      auto* engine = singleton.second;
      shutdownTasks.add("Engine [" + singleton.first + "] shutdown",
                        [&engine, singleton]()
                        {
                          engine->shutdownEngine();
                          std::ostringstream tmp2;
                          tmp2 << ANSI_FG_MAGENTA << "* Engine [" << singleton.first
                               << "] shutdown complete" << ANSI_FG_DEFAULT << '\n';
                          std::cout << tmp2.str() << std::flush;
                        });
    }

    shutdownTasks.wait();

    // STEP 5: Deleting engines. We should not delete engines before they are all shutted down
    //         because they might use other engines (for example, obsengine => geoengine).

    std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nDeleting engines" << ANSI_BOLD_OFF
              << ANSI_FG_DEFAULT << std::endl;

    for (auto& singleton : itsSingletons)
    {
      std::cout << ANSI_FG_RED << "* Deleting engine [" << singleton.first << "]\n"
                << ANSI_FG_DEFAULT;
      auto* engine = singleton.second;
      boost::this_thread::disable_interruption do_not_disturb;
      delete engine;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Shutdown operation failed!");
  }
}

Fmi::Cache::CacheStatistics Reactor::getCacheStats() const
{
  Fmi::Cache::CacheStatistics ret;

  // Engines

  for (auto engine_item : itsSingletons)
  {
    auto* engine = engine_item.second;
    if (engine && engine->ready())
    {
      Fmi::Cache::CacheStatistics engine_stats = engine->getCacheStats();
      if (!engine_stats.empty())
        ret.insert(engine_stats.begin(), engine_stats.end());
    }
  }

  // Plugins
  for (auto plugin_item : itsPlugins)
  {
    auto* smartmet_plugin = plugin_item->getPlugin();
    if (smartmet_plugin && !smartmet_plugin->isInitActive())
    {
      Fmi::Cache::CacheStatistics plugin_stats = smartmet_plugin->getCacheStats();
      ret.insert(plugin_stats.begin(), plugin_stats.end());
    }
  }

  return ret;
}

}  // namespace Spine
}  // namespace SmartMet
