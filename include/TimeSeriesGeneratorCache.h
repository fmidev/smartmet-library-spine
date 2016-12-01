// ======================================================================
/*!
 * \brief Cache for generated timeseries
 */
// ======================================================================

#pragma once

#include "TimeSeriesGenerator.h"
#include "TimeSeriesGeneratorOptions.h"
#include <macgyver/Cache.h>

namespace SmartMet
{
namespace Spine
{
class TimeSeriesGeneratorCache
{
 public:
  typedef std::shared_ptr<TimeSeriesGenerator::LocalTimeList> TimeList;

  void resize(std::size_t theSize) const;

  TimeList generate(const TimeSeriesGeneratorOptions& theOptions,
                    const boost::local_time::time_zone_ptr& theZone) const;

 private:
  mutable Fmi::Cache::Cache<std::size_t, TimeList> itsCache;
};
}  // namespace Spine
}  // namespace SmartMet
