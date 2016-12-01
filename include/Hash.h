// ======================================================================
/*!
 * \brief Extensions to boost hash
 */
// ======================================================================

#pragma once

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <cstdlib>

namespace boost
{
namespace gregorian
{
std::size_t hash_value(const date& theDate);
}
namespace posix_time
{
std::size_t hash_value(const time_duration& theDuration);
std::size_t hash_value(const ptime& theTime);
}
namespace local_time
{
std::size_t hash_value(const time_zone_ptr& theZone);
}

template <typename T>
std::size_t hash_value(const boost::optional<T>& theObj)
{
  if (theObj)
    return hash_value(*theObj);
  else
    return hash_value(false);
}
}
