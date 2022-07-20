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
#include "Thread.h"

#include <boost/function.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
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
class Reactor
{
 public:
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

  // Typedef for new instance of requested class.
  // Must be casted with reinterpret_cast<> to correct type in users code!
  using EngineInstance = void*;

  // Logging

  void setLogging(bool loggingEnabled);
  bool getLogging() const;
  bool lazyLinking() const;
  AccessLogStruct getLoggedRequests(const std::string& thePlugin) const;

  bool isLoadHigh() const;

  // Monitoring active requests

  ActiveRequests::Requests getActiveRequests() const;
  std::size_t insertActiveRequest(const HTTP::Request& theRequest);
  void removeActiveRequest(std::size_t theKey, HTTP::Status theStatusCode);

  // Monitoring active requests to backends
  void startBackendRequest(const std::string& theHost, int thePort);
  void stopBackendRequest(const std::string& theHost, int thePort);
  void resetBackendRequest(const std::string& theHost, int thePort);
  ActiveBackends::Status getBackendRequestStatus() const;

  // Only construct with options
  Reactor(Options& options);

  // Destructor
  ~Reactor();

  // No default construction, options must be given
  Reactor() = delete;

  void init();

  // Content handling

  int getRequiredAPIVersion() const;
  URIMap getURIMap() const;
  boost::optional<HandlerView&> getHandlerView(const HTTP::Request& theRequest);

  bool addContentHandler(SmartMetPlugin* thePlugin,
                         const std::string& theDir,
                         ContentHandler theCallBackFunction,
                         bool handlesUriPrefix = false);

  bool addPrivateContentHandler(SmartMetPlugin* thePlugin,
                                const std::string& theDir,
                                ContentHandler theCallBackFunction,
                                bool handlesUriPrefix = false);
  bool setNoMatchHandler(ContentHandler theHandler);
  std::size_t removeContentHandlers(SmartMetPlugin* thePlugin);

  // Plugins

  bool loadPlugin(const std::string& sectionName, const std::string& theFilename, bool verbose);
  void listPlugins() const;

  // Engines

  bool loadEngine(const std::string& sectionName, const std::string& theFilename, bool verbose);
  void listEngines() const;
  EngineInstance newInstance(const std::string& theClassName, void* user_data);
  SmartMetEngine* getSingleton(const std::string& theClassName, void* user_data);

  template <typename EngineType>
  EngineType* getEngine(const std::string& theClassName, void* user_data = nullptr);

  // Server communication

  bool addClientConnectionStartedHook(const std::string& hookName,
                                      ClientConnectionStartedHook theHook);

  bool addBackendConnectionFinishedHook(const std::string& hookName,
                                        BackendConnectionFinishedHook theHook);

  bool addClientConnectionFinishedHook(const std::string& hookName,
                                       ClientConnectionFinishedHook theHook);

  void callClientConnectionStartedHooks(const std::string& theClientIP);

  void callBackendConnectionFinishedHooks(
      const std::string& theHostName,
      int thePort,
      SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus theStatus);

  void callClientConnectionFinishedHooks(const std::string& theClientIP,
                                         const boost::system::error_code& theError);

  static bool isShuttingDown();
  void shutdown();

  bool isInitializing() const;

  Fmi::Cache::CacheStatistics getCacheStats() const;

 private:
  void initializeEngine(SmartMetEngine* theEngine, const std::string& theName);
  void initializePlugin(DynamicPlugin* thePlugin, const std::string& theName);

  /**
   *   @brief Collect information about libraries to load
   *
   *   - first element of each pair contains config section name
   *   - second element of pair contains library name (may be different when setting libname is
   *         provided
   */
  std::vector<std::pair<std::string, std::string> >
  findLibraries(const std::string& theName) const;

  bool addContentHandlerImpl(bool isPrivate,
                             SmartMetPlugin* thePlugin,
                             const std::string& theUri,
                             ContentHandler theHandler,
                             bool handlesUriPrefix);

  SmartMetEngine* getEnginePtr(const std::string& theClassName, void* user_data);

  // SmartMet API Version
  int APIVersion = SMARTMET_API_VERSION;

  const Options& itsOptions;

  // Content handling
  mutable MutexType itsContentMutex;
  using HandlerPtr = boost::shared_ptr<HandlerView>;
  using Handlers = std::map<std::string, HandlerPtr>;

  Handlers itsHandlers;

  /**
   *   @brief URI prefixes that must be linked with handlers
   *   - index is begin part of URI (for example /edr/, which could handle esim.
   *     /edr/collection/something/area?... .
   */
  std::set<std::string> uriPrefixes;

  bool itsCatchNoMatch = false;
  HandlerPtr itsCatchNoMatchHandler;

  // Filters are determined at construction, an will be stored here until inserted into the handler
  // views
  using FilterMap = std::map<std::string, boost::shared_ptr<IPFilter::IPFilter> >;
  FilterMap itsIPFilters;

  // Event hooks

  std::map<std::string, ClientConnectionStartedHook> itsClientConnectionStartedHooks;

  std::map<std::string, BackendConnectionFinishedHook> itsBackendConnectionFinishedHooks;

  std::map<std::string, ClientConnectionFinishedHook> itsClientConnectionFinishedHooks;

  mutable MutexType itsHookMutex;

  // Plugins

  using PluginList = std::list<boost::shared_ptr<DynamicPlugin> >;
  PluginList itsPlugins;

  // Engines

  // Typedefs for pointer-to-functions for easier code readability.
  using EngineNamePointer = const char* (*)();
  using EngineInstanceCreator = void* (*)(const char*, void*);
  // typedef void* (*EngineInstanceCreator)(const char*, void*);

  // List of class names and pointer-to-functions to creator functions
  using EngineList = std::map<std::string, EngineInstanceCreator>;
  EngineList itsEngines;

  using SingletonList = std::map<std::string, SmartMetEngine*>;
  SingletonList itsSingletons;

  using ConfigList = std::map<std::string, std::string>;
  ConfigList itsEngineConfigs;

  std::unique_ptr<Fmi::AsyncTaskGroup> itsInitTasks;

  // Logging

  std::atomic<bool> itsLoggingEnabled{false};
  mutable MutexType itsLoggingMutex;
  boost::posix_time::ptime itsLogLastCleaned;
  boost::shared_ptr<boost::thread> itsLogCleanerThread;

  mutable std::atomic_bool itsHighLoadFlag{false};       // is the load high
  mutable std::atomic_uint itsActiveRequestsLimit{0};    // current maximum
  mutable std::atomic_uint itsActiveRequestsCounter{0};  // requests since above was reset

  ActiveRequests itsActiveRequests;

  ActiveBackends itsActiveBackends;

 private:
  std::size_t itsEngineCount = 0;
  std::size_t itsPluginCount = 0;

  std::atomic_size_t itsInitializedPluginCount{0};
  std::atomic_size_t itsInitializedEngineCount{0};

  std::atomic_bool itsRunningAlertScript{false};

  std::atomic_bool itsInitializing{true};

  /* [[noreturn]] */ void cleanLog();
};

template <typename EngineType>
EngineType* Reactor::getEngine(const std::string& theClassName, void* user_data)
{
  static_assert(std::is_base_of<SmartMetEngine, EngineType>::value,
                "Engine class not derived from SmartMet::Spine::SmartMetEngine");
  return reinterpret_cast<EngineType*>(getEnginePtr(theClassName, user_data));
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
