#include "Hash.h"
#include "Exception.h"
// ----------------------------------------------------------------------
/*!
 * \brief Hash for boost date
 */
// ----------------------------------------------------------------------

namespace boost
{
namespace gregorian
{
std::size_t hash_value(const date& theDate)
{
  try
  {
    std::size_t seed = 0;

    if (!theDate.is_special())
    {
      hash_combine(seed, static_cast<unsigned short>(theDate.year()));
      hash_combine(seed, static_cast<unsigned short>(theDate.month()));
      hash_combine(seed, static_cast<unsigned short>(theDate.day()));
    }
    else
    {
      hash_combine(seed, theDate.is_infinity());
      hash_combine(seed, theDate.is_neg_infinity());
      hash_combine(seed, theDate.is_pos_infinity());
      hash_combine(seed, theDate.is_not_a_date());
    }
    return seed;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace gregorian
}  // namespace boost

// ----------------------------------------------------------------------
/*!
 * \brief Hash for boost time duration
 */
// ----------------------------------------------------------------------

namespace boost
{
namespace posix_time
{
std::size_t hash_value(const time_duration& theDuration)
{
  try
  {
    std::size_t seed = 0;

    if (!theDuration.is_special())
    {
      hash_combine(seed, theDuration.total_nanoseconds());
    }
    else
    {
      hash_combine(seed, theDuration.is_neg_infinity());
      hash_combine(seed, theDuration.is_pos_infinity());
      hash_combine(seed, theDuration.is_not_a_date_time());
    }
    return seed;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace posix_time
}  // namespace boost

// ----------------------------------------------------------------------
/*!
 * \brief Hash for boost::posix_time::ptime
 */
// ----------------------------------------------------------------------

namespace boost
{
namespace posix_time
{
std::size_t hash_value(const ptime& theTime)
{
  try
  {
    std::size_t seed = 0;

    if (!theTime.is_special())
    {
      hash_combine(seed, theTime.date());
      hash_combine(seed, theTime.time_of_day());
    }
    else
    {
      hash_combine(seed, theTime.is_infinity());
      hash_combine(seed, theTime.is_neg_infinity());
      hash_combine(seed, theTime.is_pos_infinity());
      hash_combine(seed, theTime.is_not_a_date_time());
    }
    return seed;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace posix_time
}  // namespace boost

// ----------------------------------------------------------------------
/*!
 * \brief Hash for boost::local_time::posix_time_zone
 *
 * Note: The posix strings may be equal even though the short hand names are different.
 *       For example Europe/Berlin, Europe/Stockholm etc
 */
// ----------------------------------------------------------------------

namespace boost
{
namespace local_time
{
std::size_t hash_value(const time_zone_ptr& theZone)
{
  try
  {
    std::size_t seed = 0xabcdef32176;
    if (theZone)
      hash_combine(seed, theZone->to_posix_string());
    return seed;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace local_time
}  // namespace boost
