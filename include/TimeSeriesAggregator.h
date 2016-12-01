// ======================================================================
/*!
 * \brief Interface of class ValueAggregator
 */
// ======================================================================

#pragma once

#include "TimeSeries.h"
#include "ParameterFunction.h"
#include "Location.h"

#include <macgyver/Stat.h>

#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
namespace TimeSeries
{
TimeSeriesPtr aggregate(const TimeSeries& ts, const ParameterFunctions& pf);
TimeSeriesGroupPtr aggregate(const TimeSeriesGroup& ts_group, const ParameterFunctions& pf);

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
