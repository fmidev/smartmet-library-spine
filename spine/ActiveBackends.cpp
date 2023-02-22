#include "ActiveBackends.h"
#include "Thread.h"

namespace SmartMet
{
namespace Spine
{
namespace
{
MutexType myMutex;
}

// ----------------------------------------------------------------------
/*!
 * \brief Start a request
 */
// ----------------------------------------------------------------------

void ActiveBackends::start(const std::string& theHost, int thePort)
{
  WriteLock lock(myMutex);
  ++itsStatus[theHost][thePort];
}

// ----------------------------------------------------------------------
/*!
 * \brief Stop a request
 */
// ----------------------------------------------------------------------

void ActiveBackends::stop(const std::string& theHost, int thePort)
{
  WriteLock lock(myMutex);
  if (itsStatus[theHost][thePort] > 0)  // safety check
    --itsStatus[theHost][thePort];
}

// ----------------------------------------------------------------------
/*!
 * \brief Reset request count
 */
// ----------------------------------------------------------------------

void ActiveBackends::reset(const std::string& theHost, int thePort)
{
  WriteLock lock(myMutex);
  itsStatus[theHost][thePort] = 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove a backend request counter. Ignore request if there is no such counter
 */
// ----------------------------------------------------------------------

void ActiveBackends::remove(const std::string& theHost, int thePort)
{
  WriteLock lock(myMutex);
  auto pos = itsStatus.find(theHost);
  if (pos != itsStatus.end())
  {
    auto& ports = pos->second;
    auto pos2 = ports.find(thePort);
    if (pos2 != ports.end())
      ports.erase(pos2);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the counters
 */
// ----------------------------------------------------------------------

ActiveBackends::Status ActiveBackends::status() const
{
  ReadLock lock(myMutex);
  return itsStatus;
}

}  // namespace Spine
}  // namespace SmartMet
