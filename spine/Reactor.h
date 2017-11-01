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

#include "ConfigBase.h"
#include "HTTP.h"
#include "HandlerView.h"
#include "IPFilter.h"
#include "LoggedRequest.h"
#include "Options.h"
#include "SmartMetPlugin.h"
#include "Thread.h"

#include <boost/function.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <libconfig.h++>

#include <list>
#include <map>
#include <string>
#include <utility>

namespace SmartMet
{
namespace Spine
{
class DynamicPlugin;

typedef std::map<std::string, std::string> URIMap;

// SmartMet base class
class Reactor
{
 public:
  typedef std::map<std::string, LogListType> LoggedRequests;
  typedef std::tuple<bool, LoggedRequests, boost::posix_time::ptime>
      AccessLogStruct;  // Fields are: Logging enabled flag, the logged requests, last cleanup time

  // These hooks are called when certain events occur with the server

  // This hook is called when a connection finishes
  // Free arguments are the backend host name, port and the final connection status code
  typedef boost::function<void(
      const std::string&, int, SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus)>
      BackendConnectionFinishedHook;

  // This hook is called when a connection starts
  // Free argument is the client ip address
  typedef boost::function<void(const std::string&)> ClientConnectionStartedHook;

  // This hook is called when a connection to the client fails
  // Free arguments are the client IP address and client connection status code
  typedef boost::function<void(const std::string&, const boost::system::error_code&)>
      ClientConnectionFinishedHook;

  // Typedef for new instance of requested class.
  // Must be casted with reinterpret_cast<> to correct type in users code!
  typedef void* EngineInstance;

  // Logging

  void setLogging(bool loggingEnabled);
  bool getLogging() const;
  bool lazyLinking() const;
  AccessLogStruct getLoggedRequests() const;

  // Only construct with options
  Reactor(Options& options);

  // Destructor
  ~Reactor();

  // Content handling

  int getRequiredAPIVersion() const;
  URIMap getURIMap() const;
  bool handle(HTTP::Request& theRequest,
              HTTP::Response& theResponse,
              const HandlerView& theHandlerView);
  boost::optional<HandlerView&> getHandlerView(const HTTP::Request& theRequest);
  bool addContentHandler(SmartMetPlugin* thePlugin,
                         const std::string& theDir,
                         ContentHandler theCallBackFunction);
  bool setNoMatchHandler(ContentHandler theHandler);

  // Plugins

  bool addPlugin(const std::string& theFilename, bool verbose);
  bool addPlugins(const std::string& theDirectory, bool verbose);
  void listPlugins() const;

  // Engines

  bool loadEngine(const std::string& theFilename, bool verbose);
  bool loadEngines(const std::string& theDirectory, bool verbose);
  void listEngines() const;
  EngineInstance newInstance(const std::string& theClassName, void* user_data);
  EngineInstance getSingleton(const std::string& theClassName, void* user_data);

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
      SmartMet::Spine::HTTP::ContentStreamer::StreamerStatus error_code);

  void callClientConnectionFinishedHooks(const std::string& theClientIP,
                                         const boost::system::error_code& theError);

  void pluginInitializedCallback(DynamicPlugin* plugin);

  bool isShutdownRequested();
  void shutdown();

 private:
  // SmartMet API Version
  int APIVersion;

  const Options& itsOptions;

  // Content handling
  mutable MutexType itsContentMutex;
  typedef std::map<std::string, boost::shared_ptr<HandlerView> > Handlers;
  Handlers itsHandlers;
  bool itsCatchNoMatch;
  boost::shared_ptr<HandlerView> itsCatchNoMatchHandler;

  // Filters are determined at construction, an will be stored here until inserted into the handler
  // views
  typedef std::map<std::string, boost::shared_ptr<IPFilter::IPFilter> > FilterMap;
  FilterMap itsIPFilters;

  // Event hooks

  std::map<std::string, ClientConnectionStartedHook> itsClientConnectionStartedHooks;

  std::map<std::string, BackendConnectionFinishedHook> itsBackendConnectionFinishedHooks;

  std::map<std::string, ClientConnectionFinishedHook> itsClientConnectionFinishedHooks;

  mutable MutexType itsHookMutex;

  // Plugins

  typedef std::list<boost::shared_ptr<DynamicPlugin> > PluginList;
  PluginList itsPlugins;

  // Engines

  // Typedefs for pointer-to-functions for easier code readability.
  typedef const char* (*EngineNamePointer)();
  typedef void* (*EngineInstanceCreator)(const char*, void*);

  // List of class names and pointer-to-functions to creator functions
  typedef std::map<std::string, EngineInstanceCreator> EngineList;
  EngineList itsEngines;

  typedef std::map<std::string, EngineInstance> SingletonList;
  SingletonList itsSingletons;

  typedef std::map<std::string, std::string> ConfigList;
  ConfigList itsEngineConfigs;

  typedef std::list<boost::shared_ptr<boost::thread> > InitThreadList;
  InitThreadList itsInitThreads;

  // Logging

  mutable MutexType itsLoggingMutex;
  bool itsLoggingEnabled;
  boost::posix_time::ptime itsLogLastCleaned;
  boost::shared_ptr<boost::thread> itsLogCleanerThread;
  bool itsShutdownRequested;

 private:
  bool pluginsLoaded = false;
  std::atomic<size_t> pluginsInitialized;
  // No void construction, options must be known
  Reactor();
  /* [[noreturn]] */ void cleanLog();
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
