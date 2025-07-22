// ======================================================================
/*!
 * \brief Implementation of class Reactor
 */
// ======================================================================

#include "Reactor.h"
#include "Backtrace.h"
#include "ConfigTools.h"
#include "Convenience.h"
#include "DynamicPlugin.h"
#include "FmiApiKey.h"
#include "HostInfo.h"
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
#include <boost/core/demangle.hpp>
#include <boost/locale.hpp>
#include <boost/make_shared.hpp>
#include <boost/regex.hpp>
#include <boost/stacktrace.hpp>
#include <boost/timer/timer.hpp>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/DateTime.h>
#include <macgyver/DebugTools.h>
#include <macgyver/Exception.h>
#include <macgyver/PostgreSQLConnection.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <filesystem>

#include <algorithm>
#include <condition_variable>
#include <dlfcn.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <mysql.h>
#include <stdexcept>
#include <string>

extern "C"
{
#include <unistd.h>
}

using namespace boost::placeholders;
using namespace std::string_literals;
namespace fs = std::filesystem;

namespace
{
std::atomic_bool gIsShuttingDown{false};
std::atomic_bool gShutdownComplete{false};

std::mutex shutdownRequestedMutex;
std::mutex shutdownFinishedMutex;
std::condition_variable shutdownRequestedCond;
std::condition_variable shutdownFinishedCond;

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
std::atomic<Reactor*> Reactor::instance{nullptr};

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Reactor::~Reactor()
{
  try
  {
    shutdown();
    if (shutdownWatchThread.joinable())
    {
      shutdownWatchThread.join();
    }
    maybeRemoveTerminateHandler();
  }
  catch (...)
  {
    std::cout << Fmi::Exception::Trace(BCP,
                                       "Exception catched in SmartMet::Spine::Raector destructor");
  }

  // Debug output
  std::cout << "SmartMet Server stopping..." << std::endl;
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct from given options
 */
// ----------------------------------------------------------------------

Reactor::Reactor(Options& options)
    : ContentHandlerMap(options), itsOptions(options), itsInitTasks(new Fmi::AsyncTaskGroup)
{
  if (instance.exchange(this))
  {
    std::cerr << "ERROR: Only one instance of SmartMet::Spine::Reactor is allowed";
    abort();
  }

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

    shutdownTimeoutSec = 60;
    options.itsConfig.lookupValue("shutdownTimeout", shutdownTimeoutSec);

    if (itsOptions.verbose)
      itsOptions.report();

    installTerminateHandler();

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
        [this](const std::string& name)
        {
          if (!initFailed.exchange(true))
          {
            auto exception =
                Fmi::Exception::Trace(BCP, "Operation failed").addParameter("Name", name);

            if (!exception.stackTraceDisabled())
              std::cerr << exception.getStackTrace();
            else if (!exception.loggingDisabled())
              std::cerr << SmartMet::Spine::log_time_str() + " Error: " + exception.what()
                        << std::endl;

            requestShutdown();
          }
        });

    shutdownWatchThread = std::thread(
        [this]() -> void
        {
          try
          {
            waitForShutdownStart();
            shutdown_impl();
            notifyShutdownComplete();
          }
          catch (...)
          {
            std::cout << Fmi::Exception::Trace(BCP, "Exception in Reactor shutdown thread")
                      << std::endl;
          }
        });

    addAdminTableRequestHandler(
        NoTarget{},
        "lastrequests",
        AdminRequestAccess::Private,
        std::bind(&Reactor::requestLastRequests, this, std::placeholders::_2),
        "Get last requests");

    addAdminTableRequestHandler(
        NoTarget{},
        "activerequests",
        AdminRequestAccess::Private,
        std::bind(&Reactor::requestActiveRequests, this, std::placeholders::_2),
        "Get active request info");

    addAdminTableRequestHandler(NoTarget{},
                                "cachestats",
                                AdminRequestAccess::Private,
                                std::bind(&Reactor::requestCacheStats, this, std::placeholders::_2),
                                "Request cache stats");

    addAdminTableRequestHandler(
        NoTarget{},
        "servicestats",
        AdminRequestAccess::Private,
        std::bind(&Reactor::requestServiceStats, this, std::placeholders::_2),
        "Request service stats");
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

      for (const auto& lib_item : libs)
        loadEngine(lib_item.first, lib_item.second, itsOptions.verbose);
    }

    // Load plugins

    if (!config.exists("plugins"))
    {
      throw Fmi::Exception(BCP, "plugins setting missing from the server configuration file");
    }

    auto libs = findLibraries("plugin");

    itsPluginCount = libs.size();

    // Then load them in parallel, keeping track of how many have been loaded
    for (const auto& lib_item : libs)
      loadPlugin(lib_item.first, lib_item.second, itsOptions.verbose);

    try
    {
      itsInitTasks->wait();
    }
    catch (...)
    {
      std::cout << ANSI_FG_RED << "* SmartMet::Spine::Reactor: initialization failed\n"
                << ANSI_FG_DEFAULT;
      throw Fmi::Exception(BCP, "At least one of initialization tasks failed");
    }
    // Set ContentEngine default logging. Do this after plugins are loaded so handlers are
    // recognized

    setLogging(itsOptions.defaultlogging);

    itsInitializing = false;
  }
  catch (...)
  {
    reportFailure("SmartMet::Spine::Reactor initialization failed");
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Find enabled libraries or plugins from the configuration
 */
// ----------------------------------------------------------------------

std::vector<std::pair<std::string, std::string> > Reactor::findLibraries(
    const std::string& theName) const
{
  const auto& name = theName;
  const auto names = theName + "s";

  const auto moduledir = itsOptions.directory + "/" + names;

  const auto& config = itsOptions.itsConfig;
  const auto& modules = config.lookup(names);

  if (!modules.isGroup())
    throw Fmi::Exception(BCP, names + "-setting must be a group of settings");

  // Collect all enabled modules

  std::vector<std::pair<std::string, std::string> > libs;

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
      libs.emplace_back(module_name, libfile);
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
 * \brief Getting lazy linking mode
 */
// ----------------------------------------------------------------------

bool Reactor::lazyLinking() const
{
  return itsOptions.lazylinking;
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

  auto now = Fmi::MicrosecClock::universal_time();

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

  // Check if we should report high load

  auto n = itsActiveRequests.size();

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
 * \brief Remove request counter to a backend
 */
// ----------------------------------------------------------------------

void Reactor::removeBackendRequests(const std::string& theHost, int thePort)
{
  itsActiveBackends.remove(theHost, thePort);
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

bool Reactor::loadPlugin(const std::string& sectionName,
                         const std::string& theFilename,
                         bool /* verbose */)
{
  try
  {
    std::string pluginname = Names::plugin_name(sectionName);

    std::string configfile;
    lookupConfigSetting(itsOptions.itsConfig, configfile, "plugins." + sectionName);

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
        itsOptions.itsConfig, filterTokens, "plugins." + sectionName + ".ip_filters");

    if (not filterTokens.empty())
    {
      std::shared_ptr<IPFilter::IPFilter> theFilter;

      try
      {
        addIPFilters(pluginname, filterTokens);
        std::cout << "IP Filter registered for plugin: " << pluginname << std::endl;
      }
      catch (std::runtime_error& err)
      {
        // No IP filter for this plugin
        std::cout << "No IP filter for plugin: " << pluginname << ". Reason: " << err.what()
                  << std::endl;
      }
    }

    std::shared_ptr<DynamicPlugin> plugin(new DynamicPlugin(theFilename, configfile, *this));

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
    reportFailure("Failed to load or init plugin");
    return false;
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
    auto it2 = itsEngineConfigs.find(name);
    if (it2 == itsEngineConfigs.end())
    {
      throw Fmi::Exception(BCP,
                           "[INTERNAL error] : itsEngineConfigs does not contain"
                           " entry for engine")
          .addParameter("theClassName", theClassName)
          .addParameter("name", name);
    }
    std::string configfile = it2->second;

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

bool Reactor::loadEngine(const std::string& sectionName,
                         const std::string& theFilename,
                         bool /* verbose */)
{
  try
  {
    // Open the dynamic library specified by private data
    // member module_filename

    std::string enginename = Names::engine_name(theFilename);

    std::string configfile;
    lookupConfigSetting(itsOptions.itsConfig, configfile, "engines." + sectionName);

    if (!configfile.empty())
    {
      absolutize_path(configfile);
      if (is_file_readable(configfile) != 0)
        throw Fmi::Exception(BCP,
                             "engine " + enginename + " config " + configfile + " is unreadable: " +
                                 std::strerror(errno));  // NOLINT not thread safe
    }

    itsEngineConfigs.insert(ConfigList::value_type(sectionName, configfile));

    void* itsHandle = dlopen(theFilename.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (itsHandle == nullptr)
    {
      // Error occurred while opening the dynamic library
      const char* err_msg_c = dlerror();  // NOLINT
      const std::string err_msg = err_msg_c ? err_msg_c : "";
      static const boost::regex r_sym("undefined\\ symbol:\\ (_Z[^\\s]*)");
      Fmi::Exception error(
          BCP,
          "Unable to load dynamic engine class library: " + err_msg);  // NOLINT not thread safe
      error.addParameter("Library", theFilename);
      boost::match_results<std::string::const_iterator> what;
      if (boost::regex_search(err_msg, what, r_sym, boost::match_default))
      {
        const std::string mangled_sym_name(what[1].first, what[1].second);
        const std::string demangled_sym_name = boost::core::demangle(mangled_sym_name.c_str());
        if (demangled_sym_name != mangled_sym_name)
        {
          error.addParameter("Demangled symbol name", demangled_sym_name);
        }
      }
      throw error;
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
    auto error = Fmi::Exception::Trace(BCP, "Operation failed!");
    error.addParameter("Section", sectionName);
    error.addParameter("Engine", theFilename);
    std::ostringstream msg;
    msg << "*** Failed to load or init engine:\n";
    msg << error;
    reportFailure(msg.str());
    return false;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize an engine
 */
// ----------------------------------------------------------------------

void Reactor::initializeEngine(SmartMetEngine* theEngine, const std::string& theName)
{
  itsInitTasks->add(
      "Load engine [" + theName + "]",
      [this, theEngine, theName]()
      {
        try
        {
          boost::timer::cpu_timer timer;
          theEngine->construct(theName, this);
          timer.stop();

          auto now_initialized = itsInitializedEngineCount.fetch_add(1) + 1;

          std::string report =
              (std::string(ANSI_FG_GREEN) + "Engine [" + theName +
               "] initialized in %t sec CPU, %w sec real (" + std::to_string(now_initialized) +
               "/" + std::to_string(itsEngineCount) + ")" + ANSI_FG_DEFAULT);

          std::cout << Spine::log_time_str() << " " << timer.format(2, report) << std::endl;

          if (now_initialized == itsEngineCount)
            std::cout << log_time_str()
                      << std::string(ANSI_FG_GREEN) + std::string(" *** All ") +
                             std::to_string(itsEngineCount) + " engines initialized" +
                             ANSI_FG_DEFAULT
                      << std::endl;
        }
        catch (...)
        {
          auto ex = Fmi::Exception::Trace(BCP, "Engine initialization failed!");
          ex.addParameter("Engine", theName);
          if (!isShuttingDown())
          {
            Fmi::Exception::ForceStackTrace force_stack_trace;
            std::cerr << ex.getStackTrace() << std::flush;
          }
          throw ex;
        }
      });
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
                                             const ClientConnectionStartedHook& theHook)
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
                                               const BackendConnectionFinishedHook& theHook)
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
                                              const ClientConnectionFinishedHook& theHook)
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
  requestShutdown();
  waitForShutdownComplete();
}

void Reactor::shutdown_impl()
{
  try
  {
    // We are no more interested about init task errors when shutdown has been requested
    itsInitTasks->stop_on_error(false);

    //---------------------------------------------------------------------------------------------
    // Perform preliminary cleanup of base class ContentHandlerMap to avoid some objects
    // staying around after plugins and engines have been deleted.
    //
    // At first clean all generic (not associated by some plugin or engine) admin request handlers
    removeContentHandlers(NoTarget{});

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
                        [this, &plugin]()
                        {
                          // Better be sure that all handlers are removed before plugin is shut down
                          removeContentHandlers(plugin->getPlugin());
                          plugin->shutdownPlugin();
                          plugin.reset();
                        });
    }

    shutdownTasks.wait();
    std::cout << ANSI_FG_RED << "* Plugin shutdown completed" << ANSI_FG_DEFAULT << std::endl;

    // STEP 4: Requesting all engines to shutdown.

    std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nShutdown engines" << ANSI_BOLD_OFF
              << ANSI_FG_DEFAULT << std::endl;

    for (const auto& singleton : itsSingletons)
    {
      std::ostringstream tmp1;
      tmp1 << ANSI_FG_RED << "* Engine [" << singleton.first << "] shutting down" << ANSI_FG_DEFAULT
           << '\n';
      std::cout << tmp1.str() << std::flush;
      auto* engine = singleton.second;
      shutdownTasks.add("Engine [" + singleton.first + "] shutdown",
                        [this, engine, singleton]()
                        {
                          // Better be sure that all handlers are removed before engine is shut down
                          removeContentHandlers(engine);
                          engine->shutdownEngine();
                          std::ostringstream tmp2;
                          tmp2 << ANSI_FG_MAGENTA << "* Engine [" << singleton.first
                               << "] shutdown complete" << ANSI_FG_DEFAULT << '\n';
                          std::cout << tmp2.str() << std::flush;
                        });
    }

    shutdownTasks.wait();

    // All init task should also be ended before we begin to destroy objects
    itsInitTasks->stop();
    itsInitTasks->wait();

    // STEP 5: Deleting engines. We should not delete engines before they are all shutted down
    //         because they might use other engines (for example, obsengine => geoengine).

    std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nDeleting engines" << ANSI_BOLD_OFF
              << ANSI_FG_DEFAULT << std::endl;

    for (const auto& singleton : itsSingletons)
    {
      std::cout << ANSI_FG_RED << "* Deleting engine [" << singleton.first << "]" << ANSI_FG_DEFAULT
                << std::endl;
      auto* engine = singleton.second;
      boost::this_thread::disable_interruption do_not_disturb;
      delete engine;
    }
    itsPlugins.clear();
    itsSingletons.clear();

    std::cout << ANSI_FG_RED << "* SmartMet::Spine::Reactor: shutdown complete" << ANSI_FG_DEFAULT
              << std::endl;
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

  for (const auto& engine_item : itsSingletons)
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
  for (const auto& plugin_item : itsPlugins)
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

void Reactor::reportFailure(const std::string& message)
{
  if (requestShutdown())
  {
    std::cerr << ANSI_FG_RED
              << "* SmartMet::Spine::Reactor: failure reported and shutdown initiated: " << message
              << ANSI_FG_DEFAULT;

    const std::exception_ptr curr_exc = std::current_exception();
    if (curr_exc)
    {
      Fmi::Exception::ForceStackTrace force_stack_trace;
      std::cerr << Fmi::Exception::Trace(BCP, "Operation failed") << std::endl;
    }
  }
}

bool Reactor::requestShutdown()
{
  std::unique_lock<std::mutex> lock(shutdownRequestedMutex);
  // Ignore call if shutdown was already requested
  bool alreadyRequested = gIsShuttingDown.exchange(true);
  if (!alreadyRequested)
  {
    Fmi::Database::PostgreSQLConnection::shutdownAll();
    shutdownRequestedCond.notify_one();
  }
  return not alreadyRequested;
}

void Reactor::waitForShutdownStart()
{
  std::unique_lock<std::mutex> lock(shutdownRequestedMutex);
  shutdownRequestedCond.wait(lock, []() -> bool { return gIsShuttingDown; });
}

void Reactor::waitForShutdownComplete()
{
  const auto complete = []() -> bool { return gShutdownComplete; };

  bool timeoutAlways = false;
  std::unique_lock<std::mutex> lock(shutdownFinishedMutex);
  for (bool done = false; not done;)
  {
    int tracerPid = Fmi::tracerPid();
    if (!timeoutAlways && tracerPid)
    {
      // Debugger attached
      std::cout << "Debugging detected (tracerPid=" << tracerPid << "). Disabled shutdown timeout."
                << std::endl;
      shutdownFinishedCond.wait(lock, complete);
      done = true;
    }
    else
    {
      // Ensure that timeout countdown is only started when shutdown is in progress
      waitForShutdownStart();

      // Now one can initiate shutdown timeout countdown
      if (shutdownFinishedCond.wait_for(lock, std::chrono::seconds(shutdownTimeoutSec), complete))
      {
        done = true;
      }
      else
      {
        // Check once more for debuggugging (one may attach debugger while shutdown is ongoing)
        if (!timeoutAlways && Fmi::tracerPid())
          continue;

        // Shutdown last for too long time
        std::cout << ANSI_FG_RED << ANSI_BOLD_ON << "\nReactor shutdown timed expired"
                  << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
        std::vector<std::string> names;
        if (itsInitTasks)
          names = itsInitTasks->active_task_names();

        if (!names.empty())
        {
          std::cout << ANSI_FG_RED << ANSI_BOLD_ON
                    << "Active Reactor initialization tasks:" << ANSI_BOLD_OFF << ANSI_FG_DEFAULT
                    << std::endl;
          for (const std::string& name : names)
            std::cout << "         " << name << std::endl;
        }
        names = shutdownTasks.active_task_names();
        if (!names.empty())
        {
          std::cout << ANSI_FG_RED << ANSI_BOLD_ON
                    << "Active Reactor shutdown tasks:" << ANSI_BOLD_OFF << ANSI_FG_DEFAULT
                    << std::endl;
          for (const std::string& name : names)
          {
            std::cout << "         " << name << std::endl;
          }
        }
        abort();
      }
    }
  }
}

bool Reactor::isShutdownFinished()
{
  return gShutdownComplete;
}

void Reactor::notifyShutdownComplete()
{
  std::unique_lock<std::mutex> lock(shutdownFinishedMutex);
  gShutdownComplete = true;
  shutdownFinishedCond.notify_all();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Admin requests handled by Reactor
//////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
std::string average_and_format(long total_microsecs, unsigned long requests)
{
  try
  {
    // Average global request time
    double average_time = total_microsecs / (1000.0 * requests);
    if (std::isnan(average_time))
      return "Not available";

    std::stringstream ss;
    ss << std::setprecision(4) << average_time;
    return ss.str();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

std::unique_ptr<Table> Reactor::requestLastRequests(const HTTP::Request& theRequest) const
try
{
  std::unique_ptr<Table> result = std::make_unique<Table>();
  std::vector<std::string> headers = {"Time", "Duration", "RequestString"};

  // Get optional parameters from the request
  const std::string s_minutes = optional_string(theRequest.getParameter("minutes"), "1");
  const std::string pluginName = optional_string(theRequest.getParameter("plugin"), "all");
  const unsigned minutes = std::min(1440UL, std::max(1UL, Fmi::stoul(s_minutes)));

  const std::string format = optional_string(theRequest.getParameter("format"), "");
  const bool decode_request = (format == "debug" || format == "html");

  result->setTitle("Last requests of " +
                   (pluginName == "all" ? "all plugins" : pluginName + " plugin") + " for last " +
                   Fmi::to_string(minutes) + " minute" + (minutes == 1 ? "" : "s"));
  result->setNames(headers);
  // Alignment in default debug format for table admin request handlers is not very usable.
  // Choose "html" instead for better readability. User may of course say otherwise, but that
  // is not handled here.
  result->setDefaultFormat("html");

  const auto currentRequests = getLoggedRequests(pluginName);

  std::size_t row = 0;
  auto firstValidTime = Fmi::SecondClock::local_time() - Fmi::Minutes(minutes);

  for (const auto& req : std::get<1>(currentRequests))
  {
    auto firstConsidered = std::find_if(req.second.begin(),
                                        req.second.end(),
                                        [&firstValidTime](const Spine::LoggedRequest& compare)
                                        { return compare.getRequestEndTime() > firstValidTime; });

    for (auto reqIt = firstConsidered; reqIt != req.second.end();
         ++reqIt)  // NOLINT(modernize-loop-convert)
    {
      const std::string uri = reqIt->getRequestString();
      std::size_t column = 0;
      std::string endtime = Fmi::to_iso_extended_string(reqIt->getRequestEndTime().time_of_day());
      std::string msec_duration = average_and_format(
          reqIt->getAccessDuration().total_microseconds(), 1);  // just format the single duration
      std::string requestString = reqIt->getRequestString();
      result->set(column++, row, endtime);
      result->set(column++, row, msec_duration);
      result->set(column++, row, decode_request ? HTTP::urldecode(uri) : uri);
      ++row;
    }
  }

  return result;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

std::unique_ptr<Table> Reactor::requestActiveRequests(const HTTP::Request& theRequest) const
{
  std::unique_ptr<Table> reqTable = std::make_unique<Table>();

  // Obtain logging information
  auto requests = getActiveRequests();

  auto now = Fmi::MicrosecClock::universal_time();

  const std::string format = optional_string(theRequest.getParameter("format"), "");
  const bool decode_request = (format == "debug" || format == "html");

  std::size_t row = 0;
  for (const auto& id_info : requests)
  {
    const auto id = id_info.first;
    const auto& time = id_info.second.time;
    const auto& req = id_info.second.request;

    const auto ip = req.getClientIP();
    const auto hostname = Spine::HostInfo::getHostName(ip);

    auto duration = now - time;

    const bool check_access_token = true;
    auto apikey = Spine::FmiApiKey::getFmiApiKey(req, check_access_token);

    auto originIP = req.getHeader("X-Forwarded-For");
    auto originhostname = ""s;
    if (originIP)
    {
      auto loc = originIP->find(',');
      if (loc == std::string::npos)
        originhostname = Spine::HostInfo::getHostName(*originIP);
      else
        originhostname = Spine::HostInfo::getHostName(originIP->substr(0, loc));
    }

    const std::string uri = req.getURI();

    std::size_t column = 0;
    reqTable->set(column++, row, Fmi::to_string(id));
    reqTable->set(column++, row, Fmi::to_iso_extended_string(time.time_of_day()));
    reqTable->set(column++, row, Fmi::to_string(duration.total_milliseconds() / 1000.0));
    reqTable->set(column++, row, ip);
    reqTable->set(column++, row, hostname);
    reqTable->set(column++, row, (originIP ? *originIP : ""s));
    reqTable->set(column++, row, originhostname);
    reqTable->set(column++, row, apikey ? *apikey : "-");
    reqTable->set(column++, row, decode_request ? HTTP::urldecode(uri) : uri);
    ++row;
  }

  reqTable->setNames({"Id",
                      "Time",
                      "Duration",
                      "ClientIP",
                      "ClientHost",
                      "OriginIP",
                      "OriginHost",
                      "Apikey",
                      "RequestString"});
  reqTable->setTitle("Active requests");
  return reqTable;
}

std::unique_ptr<Table> Reactor::requestCacheStats(const HTTP::Request& theRequest) const
{
  std::unique_ptr<Table> data_table = std::make_unique<Table>();
  data_table->setNames({"#",
                        "cache_name",
                        "maxsize",
                        "size",
                        "inserts",
                        "hits",
                        "misses",
                        "hitrate",
                        "hits/min",
                        "inserts/min",
                        "created",
                        "age"});
  data_table->setTitle("Cache statistics");

  auto now = Fmi::MicrosecClock::universal_time();
  auto cache_stats = getCacheStats();

  auto timeFormat = Spine::optional_string(theRequest.getParameter("timeformat"), "sql");
  std::unique_ptr<Fmi::TimeFormatter> timeFormatter(Fmi::TimeFormatter::create(timeFormat));

  size_t row = 1;
  for (const auto& item : cache_stats)
  {
    const auto& name = item.first;
    const auto& stat = item.second;

    data_table->set(0, row, Fmi::to_string(row));
    data_table->set(1, row, name);
    data_table->set(2, row, Fmi::to_string(stat.maxsize));
    data_table->set(3, row, Fmi::to_string(stat.size));
    data_table->set(4, row, Fmi::to_string(stat.inserts));
    data_table->set(5, row, Fmi::to_string(stat.hits));
    data_table->set(6, row, Fmi::to_string(stat.misses));

    try
    {
      auto duration = (now - stat.starttime).total_seconds();
      auto n = stat.hits + stat.misses;
      auto hit_rate = (n == 0 ? 0.0 : stat.hits * 100.0 / n);
      auto hits_per_min = (duration == 0 ? 0.0 : 60.0 * stat.hits / duration);
      auto inserts_per_min = (duration == 0 ? 0.0 : 60.0 * stat.inserts / duration);

      data_table->set(7, row, Fmi::to_string("%.1f", hit_rate));
      data_table->set(8, row, Fmi::to_string("%.1f", hits_per_min));
      data_table->set(9, row, Fmi::to_string("%.1f", inserts_per_min));
      data_table->set(10, row, timeFormatter->format(stat.starttime));
      data_table->set(11, row, Fmi::to_simple_string(now - stat.starttime));
    }
    catch (...)
    {
      data_table->set(7, row, "?");
      data_table->set(8, row, "?");
      data_table->set(9, row, "?");
      data_table->set(10, row, "?");
      data_table->set(11, row, "?");
    }
    row++;
  }
  return data_table;
}

std::unique_ptr<Table> Reactor::requestServiceStats(const HTTP::Request& theRequest) const
try
{
  const std::vector<std::string> headers{
      "Handler", "LastMinute", "LastHour", "Last24Hours", "AverageDuration"};
  std::unique_ptr<Table> statsTable = std::make_unique<Table>();
  statsTable->setTitle("Service statistics");
  statsTable->setNames(headers);

  std::string pluginName = Spine::optional_string(theRequest.getParameter("plugin"), "all");
  auto currentRequests =
      getLoggedRequests(pluginName);  // This is type tuple<bool,LogRange,DateTime>

  auto currentTime = Fmi::MicrosecClock::local_time();

  std::size_t row = 0;
  unsigned long total_minute = 0;
  unsigned long total_hour = 0;
  unsigned long total_day = 0;
  long global_microsecs = 0;

  for (const auto& reqpair : std::get<1>(currentRequests))
  {
    // Lets calculate how many hits we have in minute,hour and day and since start
    unsigned long inMinute = 0;
    unsigned long inHour = 0;
    unsigned long inDay = 0;
    long total_microsecs = 0;
    // We go from newest to oldest

    for (const auto& item : reqpair.second)
    {
      auto sinceDuration = currentTime - item.getRequestEndTime();
      auto accessDuration = item.getAccessDuration();

      total_microsecs += accessDuration.total_microseconds();

      global_microsecs += accessDuration.total_microseconds();

      if (sinceDuration < Fmi::Hours(24))
      {
        ++inDay;
        ++total_day;
        if (sinceDuration < Fmi::Hours(1))
        {
          ++inHour;
          ++total_hour;
          if (sinceDuration < Fmi::Minutes(1))
          {
            ++inMinute;
            ++total_minute;
          }
        }
      }
    }

    std::size_t column = 0;

    statsTable->set(column, row, reqpair.first);
    ++column;

    statsTable->set(column, row, Fmi::to_string(inMinute));
    ++column;

    statsTable->set(column, row, Fmi::to_string(inHour));
    ++column;

    statsTable->set(column, row, Fmi::to_string(inDay));
    ++column;

    std::string msecs = average_and_format(total_microsecs, inDay);
    statsTable->set(column, row, msecs);

    ++row;
  }

  // Finally insert totals
  std::size_t column = 0;

  statsTable->set(column, row, "Total requests");
  ++column;

  statsTable->set(column, row, Fmi::to_string(total_minute));
  ++column;

  statsTable->set(column, row, Fmi::to_string(total_hour));
  ++column;

  statsTable->set(column, row, Fmi::to_string(total_day));
  ++column;

  std::string msecs = average_and_format(global_microsecs, total_day);
  statsTable->set(column, row, msecs);

  return statsTable;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

namespace
{
// This is a static variable to hold the original terminate handler
std::atomic<Reactor*> g_reactor = nullptr;
void (*original_terminate_handler)() = nullptr;
std::atomic<bool> itsTerminateHandlerInstalled{false};
std::atomic<int> terminateCnt = 0;
std::mutex terminateHandlerMutex;
bool lastRequestsLogged = false;

[[noreturn]] void handleTerminate() noexcept
try
{
  using namespace SmartMet::Spine;

  terminateCnt += 1;
  std::unique_lock<std::mutex> lock(terminateHandlerMutex);

  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Restore the original terminate handler at first (we do not want recursion)

  std::cout << std::endl;
  std::cout << log_time_str() << ANSI_BOLD_ON << ANSI_FG_RED << " std::terminate called"
            << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
  std::cout << std::endl;

  // Check whether there is an active exception
  std::exception_ptr eptr = std::current_exception();
  if (eptr)
  {
    std::cout << log_time_str() << ANSI_BOLD_ON << ANSI_FG_RED " Uncaught exception found"
              << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
    Fmi::Exception::ForceStackTrace forceStackTrace;
    try
    {
      std::rethrow_exception(eptr);
    }
    catch (Fmi::Exception& e)
    {
      std::cout << Fmi::Exception::Trace(BCP, "Uncaught exception") << std::endl;
    }
    catch (...)
    {
      std::cout << Fmi::Exception::Trace(BCP, "Uncaught exception") << std::endl;
    }
  }

  if (!lastRequestsLogged)
  {
    // Print out the active requests (only once in case of multiple terminate calls)
    const ActiveRequests::Requests requests = g_reactor.load()->getActiveRequests();
    std::cout << log_time_str() << ANSI_BOLD_ON << ANSI_FG_RED
              << " Active requests at the time of termination" << ANSI_BOLD_OFF << ANSI_FG_DEFAULT
              << std::endl;
    std::cout << requests << std::endl;
    lastRequestsLogged = true;
  }

  const std::string backtrace = Backtrace::make_backtrace();
  if (backtrace != "")
  {
    std::cout << log_time_str() << ANSI_BOLD_ON << ANSI_FG_RED << " Backtrace\n"
              << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << backtrace << '\n'
              << std::endl;
  }

  const int prev_count = terminateCnt.fetch_sub(1);
  lock.unlock();

  if (prev_count > 1 && prev_count < 10)
  {
    // We have another pending terminate call, so put the thread to sleep for a while,
    // to get messages also from than one.
    //
    // Do not however accept to many either (10 seconds should be enough)
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::abort();
  }
  else
  {
    std::set_terminate(original_terminate_handler);
    abort();
  }
}
catch (...)
{
  // If we get here, something went wrong in the terminate handler itself

  // Restore the original terminate handler at first (we do not want recursion)
  std::set_terminate(original_terminate_handler);
  try
  {
    std::cerr << Fmi::Exception(BCP, "Exception thrown in terminate handler") << std::endl;
  }
  catch (...)
  {
  }

  std::abort();
}

}  // namespace

bool Reactor::installTerminateHandler()
{
  try
  {
    if (itsTerminateHandlerInstalled.exchange(true))
      return false;

    // Save the original terminate handler
    original_terminate_handler = std::set_terminate(handleTerminate);

    // Set the reactor pointer for use in termiante handler
    g_reactor = this;

    std::cout << log_time_str() << " Installing our terminate handler" << std::endl;
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Reactor::maybeRemoveTerminateHandler()
{
  try
  {
    const Reactor* tmp = g_reactor.load();
    if (tmp == nullptr)
      return;

    // Currently Reactor object is singleton. Play however safe if that is going to change
    if (tmp != this)
      return;

    std::cout << log_time_str() << " Restoring original terminate handler" << std::endl;
    std::set_terminate(original_terminate_handler);
    g_reactor = nullptr;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
