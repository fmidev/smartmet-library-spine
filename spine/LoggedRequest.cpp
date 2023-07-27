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
LoggedRequest::LoggedRequest(std::string theRequestString,
                             const boost::posix_time::ptime& requestEndTime,
                             boost::posix_time::time_duration accessDuration,
                             std::string theStatus,
                             std::string theIP,
                             std::string theMethod,
                             std::string theVersion,
                             std::size_t theContentLength,
                             std::string theETag,
                             std::string theApiKey)
    : itsRequestString(std::move(theRequestString)),
      itsRequestEndTime(requestEndTime),
      itsAccessDuration(std::move(accessDuration)),
      itsStatus(std::move(theStatus)),
      itsIP(std::move(theIP)),
      itsMethod(std::move(theMethod)),
      itsVersion(std::move(theVersion)),
      itsContentLength(theContentLength),
      itsETag(std::move(theETag)),
      itsApiKey(std::move(theApiKey))
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
