#pragma once

#include <string>
#include <fstream>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include "IPFilter.h"
#include "LoggedRequest.h"
#include "SmartMetPlugin.h"
#include "HTTP.h"
#include "Thread.h"

namespace SmartMet
{
namespace Spine
{
// Content handling typedefs

class Reactor;

typedef boost::function<void(Reactor&, const HTTP::Request&, HTTP::Response&)> ContentHandler;

// Logging-related typedefs
typedef std::list<LoggedRequest> LogListType;
typedef std::map<std::string, LogListType> LoggedRequests;
typedef std::tuple<bool, LoggedRequests, boost::posix_time::ptime> AccessLogStruct;  // Fields are:
                                                                                     // Logging
                                                                                     // enabled
                                                                                     // flag, the
                                                                                     // logged
                                                                                     // requests,
                                                                                     // last cleanup
                                                                                     // time

// AccessLogger handles a file for access logs and rotation
class AccessLogger
{
 public:
  AccessLogger(const std::string& resource, const std::string& accessLogDir);

  ~AccessLogger();

  void log(const LoggedRequest& theRequest);

  void start();

  void stop();

 private:
  std::string itsResource;

  std::string itsLoggingDir;

  std::unique_ptr<std::ofstream> itsFileHandle;

  bool itsIsRunning;
};

class HandlerView : private boost::noncopyable
{
 public:
  // Regular plugin
  HandlerView(ContentHandler theHandler,
              boost::shared_ptr<IPFilter::IPFilter> theIpFilter,
              SmartMetPlugin* thePlugin,
              const std::string& theResource,
              bool loggingStatus,
              const std::string& accessLogDir);

  // CatchNoMatch handler
  HandlerView(ContentHandler theHandler);

  // Destructor
  ~HandlerView();

  // Handle a request
  bool handle(Reactor& theReactor, const HTTP::Request& theRequest, HTTP::Response& theResponse);

  // See if this View is that of a CatchNoMatch
  bool isCatchNoMatch() const;

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
  LogListType getLoggedRequests();

 private:
  // The actual handler functor
  ContentHandler itsHandler;

  // Any registered IP Filters
  boost::shared_ptr<IPFilter::IPFilter> itsIpFilter;

  // Non-owning pointer to the plugin
  SmartMetPlugin* itsPlugin;

  // The base uri registered for this handler
  std::string itsResource;

  // Flag to see if this is the fallthrough-handler
  bool itsIsCatchNoMatch;

  // The request log for this handler
  LogListType itsRequestLog;

  // Mutex for logging operations
  MutexType itsLoggingMutex;

  // Flag to see if logging is on
  bool isLogging;

  // Iterator to point to the last request flushed to disk
  LogListType::iterator itsLastFlushedRequest;

  // Handle for access log file
  std::unique_ptr<AccessLogger> itsAccessLog;
};

}  // namespace Spine
}  // namespace SmartMet
