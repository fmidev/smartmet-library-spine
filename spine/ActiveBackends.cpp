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
