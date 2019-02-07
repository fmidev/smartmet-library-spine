#include "ActiveBackends.h"

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Start a request
 */
// ----------------------------------------------------------------------

void ActiveBackends::start(const std::string& theHost, int thePort)
{
  WriteLock lock(itsMutex);
  ++itsStatus[theHost][thePort];
}

// ----------------------------------------------------------------------
/*!
 * \brief Stop a request
 */
// ----------------------------------------------------------------------

void ActiveBackends::stop(const std::string& theHost, int thePort)
{
  WriteLock lock(itsMutex);
  --itsStatus[theHost][thePort];
}

// ----------------------------------------------------------------------
/*!
 * \brief Reset request count
 */
// ----------------------------------------------------------------------

void ActiveBackends::reset(const std::string& theHost, int thePort)
{
  WriteLock lock(itsMutex);
  itsStatus[theHost][thePort] = 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the counters
 */
// ----------------------------------------------------------------------

ActiveBackends::Status ActiveBackends::status() const
{
  ReadLock lock(itsMutex);
  return itsStatus;
}

}  // namespace Spine
}  // namespace SmartMet
