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
typedef boost::variant<None, std::string, double, int, LonLat, boost::local_time::local_date_time>
    Value;

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
typedef std::vector<TimedValue> TimeSeries;
typedef boost::shared_ptr<TimeSeries> TimeSeriesPtr;

// time series result variable for an area
struct LonLatTimeSeries
{
  LonLatTimeSeries(const LonLat& coord, const TimeSeries& ts) : lonlat(coord), timeseries(ts) {}
  LonLat lonlat;
  TimeSeries timeseries;
};

// several coordinate-time series pairs
typedef std::vector<LonLatTimeSeries> TimeSeriesGroup;
typedef boost::shared_ptr<TimeSeriesGroup> TimeSeriesGroupPtr;

// time series vector
typedef std::vector<TimeSeries> TimeSeriesVector;
typedef boost::shared_ptr<TimeSeriesVector> TimeSeriesVectorPtr;

template <>
bool None::operator==(const None& other) const;

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
