#pragma once

#include "AccessLogger.h"
#include "HTTP.h"
#include "IPFilter.h"
#include "LogRange.h"
#include "SmartMetPlugin.h"
#include "Thread.h"
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

namespace SmartMet
{
namespace Spine
{
// Content handling typedefs

class Reactor;
class AccessLogger;

using ContentHandler = boost::function<void(Reactor&, const HTTP::Request&, HTTP::Response&)>;

class HandlerView
{
 public:
  // Regular plugin
  HandlerView(ContentHandler theHandler,
              boost::shared_ptr<IPFilter::IPFilter> theIpFilter,
              SmartMetPlugin* thePlugin,
              const std::string& theResource,
              bool loggingStatus,
              bool isprivate,
              const std::string& accessLogDir);

  // CatchNoMatch handler
  explicit HandlerView(ContentHandler theHandler);

  // Destructor
  ~HandlerView();

  HandlerView(const HandlerView& other) = delete;
  HandlerView(HandlerView&& other) = delete;
  HandlerView& operator=(const HandlerView& other) = delete;
  HandlerView& operator=(HandlerView&& other) = delete;

  // Handle a request
  bool handle(Reactor& theReactor, const HTTP::Request& theRequest, HTTP::Response& theResponse);

  // See if this View is that of a CatchNoMatch
  bool isCatchNoMatch() const;

  // See if handler is private
  bool isPrivate() const { return itsPrivate; }

  // See if querying this plugin is fast
  bool queryIsFast(HTTP::Request& theRequest) const;

  // Get the name of the associated plugin
  std::string getPluginName() const;

  // Set logging status for this handler
  void setLogging(bool newStatus);

  // Get logging status
  bool getLogging();

  // Clean too old entries from the log list
  void cleanLog(const boost::posix_time::ptime& minTime);

  // Flush pending requests to disk
  void flushLog();

  // Get logged requests
  LogRange getLoggedRequests();

  // Relase a log range
  void releaseLogRange();

  // Lock a log range
  void lockLogRange();

  // Check whether handler uses specified plugin
  bool usesPlugin(const SmartMetPlugin* plugin) const;

  // Get URI
  const std::string& getResource() const;

 private:
  // The actual handler functor
  ContentHandler itsHandler;

  // Any registered IP Filters
  boost::shared_ptr<IPFilter::IPFilter> itsIpFilter;

  // Non-owning pointer to the plugin
  SmartMetPlugin* itsPlugin = nullptr;

  // The base uri registered for this handler
  std::string itsResource;

  // Flag to see if this is the fallthrough-handler
  bool itsIsCatchNoMatch = false;

  // Flag to specify that handler is private
  bool itsPrivate = false;

  // The request log for this handler
  LogListType itsRequestLog;

  // Mutex for logging operations
  MutexType itsLoggingMutex;

  // Flag to see if logging is on
  bool isLogging = false;

  // How many LogRanges are in use
  std::atomic<int> itsLogReaderCount{0};

  // Iterator to point to the last request flushed to disk
  LogListType::iterator itsLastFlushedRequest;

  // Handle for access log file
  std::unique_ptr<AccessLogger> itsAccessLog;
};

}  // namespace Spine
}  // namespace SmartMet
