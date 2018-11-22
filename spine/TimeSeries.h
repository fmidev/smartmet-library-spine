// ======================================================================
/*!
 * \brief Interface of class ValueAggregator
 */
// ======================================================================

#pragma once

#include <string>
#include <vector>

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/variant.hpp>

#include "Table.h"
#include "ValueFormatter.h"

namespace SmartMet
{
namespace Spine
{
namespace TimeSeries
{
struct LonLat
{
  LonLat(double longitude, double latitude) : lon(longitude), lat(latitude) {}
  double lon;
  double lat;
};

bool operator==(const LonLat& lonlat1, const LonLat& lonlat2);

// Type to indicate empty ('nan') result
struct None
{
  // Nans are always identical to themselves, no nothing else
  template <class T>
  bool operator==(const T& other) const;
};

// data variable for qengine, obsengine
using Value =
    boost::variant<None, std::string, double, int, LonLat, boost::local_time::local_date_time>;

enum class TimeSeriesValueType
{
  None,
  String,
  Double,
  Int,
  LonLat,
  DateTime
};

// time series variable for a point
struct TimedValue
{
  TimedValue(const boost::local_time::local_date_time& timestamp, Value val)
      : time(timestamp), value(val)
  {
  }
  boost::local_time::local_date_time time;
  Value value;
};

// one time series
using TimeSeries = std::vector<TimedValue>;
using TimeSeriesPtr = boost::shared_ptr<TimeSeries>;

// time series result variable for an area
struct LonLatTimeSeries
{
  LonLatTimeSeries(const LonLat& coord, const TimeSeries& ts) : lonlat(coord), timeseries(ts) {}
  LonLat lonlat;
  TimeSeries timeseries;
};

// several coordinate-time series pairs
using TimeSeriesGroup = std::vector<LonLatTimeSeries>;
using TimeSeriesGroupPtr = boost::shared_ptr<TimeSeriesGroup>;

// time series vector
using TimeSeriesVector = std::vector<TimeSeries>;
using TimeSeriesVectorPtr = boost::shared_ptr<TimeSeriesVector>;

template <>
bool None::operator==(const None& other) const;

TimeSeriesValueType valueType(const TimeSeries& ts);
TimeSeriesValueType valueType(const Value& val);

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
