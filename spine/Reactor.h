// ======================================================================
/*!
 * \brief Interface of class Reactor
 *
 * The Reactor class may be either a forebrain or a hindbrain server,
 * everything depends on the *definition* by the user of this declaration.
 * In practise the user is expected to include this file and then
 * define all the methods declared in this header.
 *
 */
// ======================================================================

#pragma once

#include "ActiveBackends.h"
#include "ActiveRequests.h"
#include "ConfigBase.h"
#include "HTTP.h"
#include "HandlerView.h"
#include "IPFilter.h"
#include "LoggedRequest.h"
#include "Options.h"
#include "SmartMet.h"
#include "SmartMetEngine.h"
#include "SmartMetPlugin.h"
#include "ContentHandlerMap.h"
#include "Thread.h"

#include <boost/function.hpp>
#include <optional>
#include <memory>
#include <macgyver/AsyncTaskGroup.h>
#include <macgyver/CacheStats.h>

#include <atomic>
#include <libconfig.h++>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace SmartMet
{
namespace Spine
{
class DynamicPlugin;

using URIMap = std::map<std::string, std::string>;

// SmartMet base class
class Reactor final : public ContentHandlerMap
{
 public:
  static std::atomic<Reactor*> instance;

  // These hooks are called when certain events occur with the server

  // This hook is called when a connection finishes
  // Free arguments are the backend host name, port and the final connection status code
  using BackendConnectionFinishedHook = boost::function<void(
      const std::string&, int, SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus)>;

  // This hook is called when a connection starts
  // Free argument is the client ip address
  using ClientConnectionStartedHook = boost::function<void(const std::string&)>;

  // This hook is called when a connection to the client fails
  // Free arguments are the client IP address and client connection status code
  using ClientConnectionFinishedHook =
      boost::function<void(const std::string&, const boost::system::error_code&)>;

    // Logging

  bool lazyLinking() const;

  bool isLoadHigh() const;

  // Monitoring active requests

  ActiveRequests::Requests getActiveRequests() const;
  std::size_t insertActiveRequest(const HTTP::Request& theRequest);
  void removeActiveRequest(std::size_t theKey, HTTP::Status theStatusCode);

  // Monitoring active requests to backends
  void startBackendRequest(const std::string& theHost, int thePort);
  void stopBackendRequest(const std::string& theHost, int thePort);
  void resetBackendRequest(const std::string& theHost, int thePort);
  void removeBackendRequests(const std::string& theHost, int thePort);
  ActiveBackends::Status getBackendRequestStatus() const;

  // Only construct with options
  explicit Reactor(Options& options);

  // Destructor
  ~Reactor();

  // No default construction, options must be given
  Reactor() = delete;

  void init();

  // Content handling

  bool isEncrypted() const { return itsOptions.encryptionEnabled; }

  int getRequiredAPIVersion() const;

    /**
   *   @brief Static method for reporting failure which requires Reactor shutdown
   */
  static void reportFailure(const std::string& message);

  // Plugins

  bool loadPlugin(const std::string& sectionName, const std::string& theFilename, bool verbose);
  void listPlugins() const;

  // Engines

  bool loadEngine(const std::string& sectionName, const std::string& theFilename, bool verbose);
  void listEngines() const;
  std::shared_ptr<SmartMetEngine> newInstance(const std::string& theClassName, void* user_data);
  std::shared_ptr<SmartMetEngine> getSingleton(const std::string& theClassName, void* user_data);

  template <typename EngineType>
  typename std::enable_if<std::is_base_of<SmartMetEngine, EngineType>::value, std::shared_ptr<EngineType>>::type
  getEngine(const std::string& theClassName, void* user_data = nullptr);

  // Server communication

  bool addClientConnectionStartedHook(const std::string& hookName,
                                      const ClientConnectionStartedHook& theHook);

  bool addBackendConnectionFinishedHook(const std::string& hookName,
                                        const BackendConnectionFinishedHook& theHook);

  bool addClientConnectionFinishedHook(const std::string& hookName,
                                       const ClientConnectionFinishedHook& theHook);

  void callClientConnectionStartedHooks(const std::string& theClientIP);

  void callBackendConnectionFinishedHooks(
      const std::string& theHostName,
      int thePort,
      SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus theStatus);

  void callClientConnectionFinishedHooks(const std::string& theClientIP,
                                         const boost::system::error_code& theError);

  bool isInitializing() const;

  Fmi::Cache::CacheStatistics getCacheStats() const;

  //---------------------  Reactor shutdown support  ----------------------------
  /**
   *   @brief Reactor shutdown support
   */
  //@{

  static bool isShuttingDown();

  /**
   *  @brief Request reactor shutdown and wait for shutdown to complete
   */
  void shutdown();

  /**
   *  @brief Request reactor shutdown
   *
   *  @retval @c true shutdown initiated
   *  @retval @c false call ignored because of shutdown already requested earlier
   */
  static bool requestShutdown();

  void waitForShutdownComplete();

  static bool isShutdownFinished();

  /**
   * @brief Set callback for shutdown timeout
   *
   * Default (empty std::function) will cause abort() to be called on timeout
   * abort() will also be called if provided callback throws an exception or exits.
   */
  inline void onShutdownTimedOut(std::function<void()> callback)
  {
    shutdownTimedOutCallback = callback;
  }

 private:

  /**
   *  @brief Method called when initialization is done
   *
   *  Convenient place where to put breakpoint when debugging
   */
  static void initDone();

  /**
   *  @brief Actual reactor shutdown implementation
   */
  void shutdown_impl();

  void waitForShutdownStart();

  void notifyShutdownComplete();

  std::thread shutdownWatchThread;
  std::unique_ptr<Table> requestLastRequests(const HTTP::Request& theRequest) const;

  std::unique_ptr<Table> requestActiveRequests(const HTTP::Request& theRequest) const;

  std::unique_ptr<Table> requestCacheStats(const HTTP::Request& theRequest) const;

  std::unique_ptr<Table> requestServiceStats(const HTTP::Request& theRequest) const;

  std::unique_ptr<Table> requestEngineInfo(const HTTP::Request& theRequest) const;

  std::unique_ptr<Table> requestPluginInfo(const HTTP::Request& theRequest) const;

  /**
   * @brief Install handler for cases when std::terminate is called
   *
   * Actions on std::terminate:
   * - log that std::terminate is called
   * - check whether there is an active exception and log information about it if
   *   found (may happen when some thread procedure ends with unhandled exception or
   *   when std::terminate is called from nothrow method due to unhandled exception)
   * - try to log all active requests
   * - call original std::terminate
   *
   * @retval true Handler installed successfully
   * @retval false Handler already installed
   **/
  bool installTerminateHandler();

  void maybeRemoveTerminateHandler();

  Fmi::AsyncTaskGroup shutdownTasks;

  unsigned int shutdownTimeoutSec;

  //@}
  //------------------------------------------------------------------------------

  void initializeEngine(SmartMetEngine* theEngine, const std::string& theName);
  void initializePlugin(DynamicPlugin* thePlugin, const std::string& theName);

  /**
   *   @brief Collect information about libraries to load
   *
   *   - first element of each pair contains config section name
   *   - second element of pair contains library name (may be different when setting libname is
   *         provided
   */
  std::vector<std::pair<std::string, std::string> > findLibraries(const std::string& theName) const;

  std::shared_ptr<SmartMetEngine> getEnginePtr(const std::string& theClassName, void* user_data);

  // SmartMet API Version
  int APIVersion = SMARTMET_API_VERSION;

  const Options& itsOptions;

  // Content handling
  using HandlerPtr = std::shared_ptr<HandlerView>;

  // Event hooks

  std::map<std::string, ClientConnectionStartedHook> itsClientConnectionStartedHooks;

  std::map<std::string, BackendConnectionFinishedHook> itsBackendConnectionFinishedHooks;

  std::map<std::string, ClientConnectionFinishedHook> itsClientConnectionFinishedHooks;

  mutable MutexType itsHookMutex;
  mutable boost::mutex itsInitMutex;

  using PluginList = std::list<std::shared_ptr<DynamicPlugin> >;
  PluginList itsPlugins;

  // Engines

  // Typedefs for pointer-to-functions for easier code readability.
  using EngineNamePointer = const char* (*)();
  using EngineInstanceCreator = void* (*)(const char*, void*);
  // typedef void* (*EngineInstanceCreator)(const char*, void*);

  // List of class names and pointer-to-functions to creator functions
  using EngineList = std::map<std::string, EngineInstanceCreator>;
  EngineList itsEngines;

  using SingletonList = std::map<std::string, std::shared_ptr<SmartMetEngine>>;
  SingletonList itsSingletons;

  using ConfigList = std::map<std::string, std::string>;
  ConfigList itsEngineConfigs;

  std::unique_ptr<Fmi::AsyncTaskGroup> itsInitTasks;

  mutable std::atomic_bool itsHighLoadFlag{false};       // is the load high
  mutable std::atomic_uint itsActiveRequestsLimit{0};    // current maximum
  mutable std::atomic_uint itsActiveRequestsCounter{0};  // requests since above was reset

  ActiveRequests itsActiveRequests;

  ActiveBackends itsActiveBackends;

  std::size_t itsEngineCount = 0;
  std::size_t itsPluginCount = 0;

  std::atomic_size_t itsInitializedPluginCount{0};
  std::atomic_size_t itsInitializedEngineCount{0};

  std::atomic_bool itsRunningAlertScript{false};

  std::atomic_bool itsInitializing{true};

  std::atomic_bool initFailed{false};

  /* [[noreturn]] */ void cleanLog();

  std::function<void()> shutdownTimedOutCallback;
};

template <typename EngineType>
typename std::enable_if<std::is_base_of<SmartMetEngine, EngineType>::value, std::shared_ptr<EngineType>>::type
Reactor::getEngine(const std::string& theClassName, void* user_data)
{
  std::shared_ptr<EngineType> engine = std::dynamic_pointer_cast<EngineType>(getEnginePtr(theClassName, user_data));
  if (!engine)
  {
    Fmi::Exception err(BCP, "Dynamic engine type cast failed");
    err.addParameter("Requested engine class name", theClassName);
    throw err;
  }
  return engine;
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
