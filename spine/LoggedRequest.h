#pragma once

/* -----------------------------------------------
 * LoggedRequest struct stores request information
 * -----------------------------------------------
 */

#include <macgyver/DateTime.h>
#include <string>

#include "HTTP.h"

namespace SmartMet
{
namespace Spine
{
class LoggedRequest
{
 public:
  // Eleven-arg form is the new canonical constructor; `cpuDuration`
  // is the per-thread CPU time spent in the handler, distinct from
  // `accessDuration` which is the wall-clock elapsed time. Used by
  // ?what=servicestats to expose AverageCPUMs alongside
  // AverageDuration so operators can read CPU-bound vs wait-bound
  // handlers at a glance.
  LoggedRequest(std::string theRequestString,
                const Fmi::DateTime& requestEndTime,
                Fmi::TimeDuration accessDuration,
                Fmi::TimeDuration cpuDuration,
                std::string theStatus,
                std::string theIP,
                std::string theMethod,
                std::string theVersion,
                std::size_t theContentLength,
                std::string theETag,
                std::string theApiKey);

  // Backwards-compatible 10-arg form: any caller (none in tree but
  // out-of-tree plugins might exist) using the historical signature
  // gets cpuDuration = 0. Delegates to the canonical form.
  LoggedRequest(std::string theRequestString,
                const Fmi::DateTime& requestEndTime,
                Fmi::TimeDuration accessDuration,
                std::string theStatus,
                std::string theIP,
                std::string theMethod,
                std::string theVersion,
                std::size_t theContentLength,
                std::string theETag,
                std::string theApiKey);

  Fmi::DateTime getRequestEndTime() const;
  Fmi::DateTime getRequestStartTime() const;

  Fmi::TimeDuration getAccessDuration() const;
  Fmi::TimeDuration getCpuDuration() const;

  std::string getRequestString() const;
  std::string getStatus() const;
  std::string getIP() const;
  std::string getMethod() const;
  std::string getVersion() const;
  std::size_t getContentLength() const;
  std::string getETag() const;
  std::string getApiKey() const;

 private:
  std::string itsRequestString;
  Fmi::DateTime itsRequestEndTime;
  Fmi::TimeDuration itsAccessDuration;
  Fmi::TimeDuration itsCpuDuration;
  std::string itsStatus;
  std::string itsIP;
  std::string itsMethod;
  std::string itsVersion;
  std::size_t itsContentLength;
  std::string itsETag;
  std::string itsApiKey;
};

using LogListType = std::list<LoggedRequest>;

}  // namespace Spine
}  // namespace SmartMet
