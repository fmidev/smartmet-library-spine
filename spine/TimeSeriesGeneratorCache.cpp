#include "TimeSeriesGeneratorCache.h"
#include "Exception.h"
#include <boost/functional/hash.hpp>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Resize the cache
 *
 * The user is expected to resize the cache to a user configurable
 * value during startup.
 */
// ----------------------------------------------------------------------

void TimeSeriesGeneratorCache::resize(std::size_t theSize) const
{
  try
  {
    itsCache.resize(theSize);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the time series
 *
 * Use cache if possible, otherwise generate and cache
 */
// ----------------------------------------------------------------------

TimeSeriesGeneratorCache::TimeList TimeSeriesGeneratorCache::generate(
    const TimeSeriesGeneratorOptions& theOptions,
    const boost::local_time::time_zone_ptr& theZone) const
{
  try
  {
    // hash value for the query
    std::size_t hash = theOptions.hash_value();
    boost::hash_combine(hash, boost::hash_value(theZone));

    // use cached result if possible
    auto cached_result = itsCache.find(hash);
    if (cached_result)
      return *cached_result;

    // generate time series and cache it for future use
    TimeList series(
        new TimeSeriesGenerator::LocalTimeList(TimeSeriesGenerator::generate(theOptions, theZone)));
    itsCache.insert(hash, series);
    return series;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet
