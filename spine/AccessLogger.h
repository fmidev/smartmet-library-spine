#pragma once

#include <fstream>
#include <list>
#include <string>

#include "LoggedRequest.h"

namespace SmartMet
{
namespace Spine
{
// Logging-related typedefs
typedef std::list<LoggedRequest> LogListType;
typedef std::map<std::string, LogListType> LoggedRequests;

// Fields are: Logging enabled flag, the logged requests, last cleanup time
typedef std::tuple<bool, LoggedRequests, boost::posix_time::ptime> AccessLogStruct;

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

}  // namespace Spine
}  // namespace SmartMet
