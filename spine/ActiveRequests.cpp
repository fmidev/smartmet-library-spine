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

std::size_t ActiveRequests::insert(const std::string& theURI)
{
  WriteLock lock(itsMutex);
  auto key = ++itsCounter;
  Info info{theURI, boost::posix_time::microsec_clock::universal_time()};
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

}  // namespace Spine
}  // namespace SmartMet
