#pragma once

/* -----------------------------------------------
 * LoggedRequest struct stores request information
 * -----------------------------------------------
 */

#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>

#include "HTTP.h"

namespace SmartMet
{
namespace Spine
{
class LoggedRequest
{
 public:
  LoggedRequest(const std::string& theRequestString,
                const boost::posix_time::ptime& requestTime,
                const boost::posix_time::time_duration& accessDuration,
                const std::string& theStatus,
                const std::string& theIP,
                const std::string& theMethod,
                const std::string& theVersion);

  boost::posix_time::ptime getRequestTime() const;

  boost::posix_time::time_duration getAccessDuration() const;

  std::string getRequestString() const;

  std::string getStatus() const;

  std::string getIP() const;

  std::string getMethod() const;

  std::string getVersion() const;

 private:
  std::string itsRequestString;

  boost::posix_time::ptime itsRequestTime;

  boost::posix_time::time_duration itsAccessDuration;

  std::string itsStatus;

  std::string itsIP;

  std::string itsMethod;

  std::string itsVersion;
};

}  // namespace Spine
}  // namespace SmartMet