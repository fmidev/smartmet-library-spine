#include "ActiveRequests.h"

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Add a new active request
 */
// ----------------------------------------------------------------------

std::size_t ActiveRequests::insert(const HTTP::Request& theRequest)
{
  WriteLock lock(itsMutex);
  auto key = ++itsStartedCounter;
  Info info{theRequest, boost::posix_time::microsec_clock::universal_time()};
  itsRequests.insert(Requests::value_type{key, info});
  return key;
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove an active request
 */
// ----------------------------------------------------------------------

void ActiveRequests::remove(std::size_t theKey)
{
  WriteLock lock(itsMutex);
  auto pos = itsRequests.find(theKey);
  if (pos != itsRequests.end())
    itsRequests.erase(pos);
  ++itsFinishedCounter;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return information on active requests
 */
// ----------------------------------------------------------------------

ActiveRequests::Requests ActiveRequests::requests() const
{
  WriteLock lock(itsMutex);
  auto ret = itsRequests;
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the number of active requests
 */
// ----------------------------------------------------------------------

std::size_t ActiveRequests::size() const
{
  WriteLock lock(itsMutex);
  return itsRequests.size();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return number of requests completed
 */
// ----------------------------------------------------------------------

std::size_t ActiveRequests::counter() const
{
  return itsFinishedCounter;
}

}  // namespace Spine
}  // namespace SmartMet
