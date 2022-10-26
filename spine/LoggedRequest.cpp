#include "LoggedRequest.h"

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------
LoggedRequest::LoggedRequest(const std::string& theRequestString,
                             const boost::posix_time::ptime& requestEndTime,
                             const boost::posix_time::time_duration& accessDuration,
                             const std::string& theStatus,
                             const std::string& theIP,
                             const std::string& theMethod,
                             const std::string& theVersion,
                             std::size_t theContentLength,
                             const std::string& theETag,
                             const std::string& theApiKey)
    : itsRequestString(theRequestString),
      itsRequestEndTime(requestEndTime),
      itsAccessDuration(accessDuration),
      itsStatus(theStatus),
      itsIP(theIP),
      itsMethod(theMethod),
      itsVersion(theVersion),
      itsContentLength(theContentLength),
      itsETag(theETag),
      itsApiKey(theApiKey)
{
}

boost::posix_time::ptime LoggedRequest::getRequestEndTime() const
{
  return itsRequestEndTime;
}

boost::posix_time::ptime LoggedRequest::getRequestStartTime() const
{
  return itsRequestEndTime - itsAccessDuration;
}

std::string LoggedRequest::getRequestString() const
{
  return itsRequestString;
}

boost::posix_time::time_duration LoggedRequest::getAccessDuration() const
{
  return itsAccessDuration;
}

std::string LoggedRequest::getStatus() const
{
  return itsStatus;
}

std::string LoggedRequest::getIP() const
{
  return itsIP;
}

std::string LoggedRequest::getMethod() const
{
  return itsMethod;
}

std::string LoggedRequest::getVersion() const
{
  return itsVersion;
}

std::string LoggedRequest::getETag() const
{
  return itsETag;
}

std::string LoggedRequest::getApiKey() const
{
  return itsApiKey;
}

std::size_t LoggedRequest::getContentLength() const
{
  return itsContentLength;
}

}  // namespace Spine
}  // namespace SmartMet
