// ======================================================================
/*!
 * \brief Time series generator for forecasts and observations
 */
// ======================================================================

#pragma once

#include "TimeSeriesGeneratorOptions.h"

#include <boost/date_time/local_time/local_time.hpp>
#include <list>
#include <string>

namespace SmartMet
{
namespace Spine
{
namespace TimeSeriesGenerator
{
typedef std::list<boost::local_time::local_date_time> LocalTimeList;

LocalTimeList generate(const TimeSeriesGeneratorOptions& theOptions,
                       const boost::local_time::time_zone_ptr& theZone);

}  // namespace TimeSeriesGenerator
}  // namespace Spine
}  // namespace SmartMet