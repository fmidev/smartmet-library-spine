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
                             const std::string& theVersion)
    : itsRequestString(theRequestString),
      itsRequestEndTime(requestEndTime),
      itsAccessDuration(accessDuration),
      itsStatus(theStatus),
      itsIP(theIP),
      itsMethod(theMethod),
      itsVersion(theVersion)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Get request end timestamp
 */
// ----------------------------------------------------------------------

boost::posix_time::ptime LoggedRequest::getRequestEndTime() const
{
  return itsRequestEndTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get request start timestamp
 */
// ----------------------------------------------------------------------

boost::posix_time::ptime LoggedRequest::getRequestStartTime() const
{
  return itsRequestEndTime - itsAccessDuration;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get request RequestString
 */
// ----------------------------------------------------------------------

std::string LoggedRequest::getRequestString() const
{
  return itsRequestString;
}
// ----------------------------------------------------------------------
/*!
 * \brief Get request access duration
 */
// ----------------------------------------------------------------------

boost::posix_time::time_duration LoggedRequest::getAccessDuration() const
{
  return itsAccessDuration;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get request status
 */
// ----------------------------------------------------------------------

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

}  // namespace Spine
}  // namespace SmartMet
