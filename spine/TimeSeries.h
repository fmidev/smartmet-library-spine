// ======================================================================
/*!
 * \brief Interface of class ValueAggregator
 */
// ======================================================================

#pragma once

#include <string>
#include <vector>
#include <map>

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


// Local time pool
class LocalTimePool
{
public:  
  const boost::local_time::local_date_time& create(const boost::posix_time::ptime& t, const boost::local_time::time_zone_ptr& tz);
  size_t size() const;
  void print(std::ostream& os) const;
private:
  std::map<std::string, boost::local_time::local_date_time> localtimes;
};

using LocalTimePoolPtr = boost::shared_ptr<LocalTimePool>;

struct TimedValue
{
  TimedValue(const boost::local_time::local_date_time& timestamp, const Value& val)
	: time(const_cast<boost::local_time::local_date_time&>(timestamp)), value(val)
  {
  }
  TimedValue(const TimedValue& tv) : time(const_cast<boost::local_time::local_date_time&>(tv.time)), value(tv.value) {}
  TimedValue& operator=(const TimedValue& tv) { time = const_cast<boost::local_time::local_date_time&>(tv.time); value = tv.value; return *this; }

  boost::local_time::local_date_time& time;
  Value value;
};

using TimedValueVector = std::vector<TimedValue>;

class TimeSeries : public TimedValueVector
{
public:
  TimeSeries(LocalTimePoolPtr time_pool);
  void emplace_back(const TimedValue& tv);
  void push_back(const TimedValue& tv);
  TimedValueVector::iterator insert(TimedValueVector::iterator pos, const TimedValue& tv);
  void insert(TimedValueVector::iterator pos, TimedValueVector::iterator first, TimedValueVector::iterator last);
  void insert(TimedValueVector::iterator pos, TimedValueVector::const_iterator first, TimedValueVector::const_iterator last);
  TimeSeries& operator=(const TimeSeries& ts);
  
  LocalTimePoolPtr getLocalTimePool() const { return local_time_pool; }

private:
  LocalTimePoolPtr local_time_pool{nullptr};
};

// one time series
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

std::ostream& operator<<(std::ostream& os, const LocalTimePool& localTimePool);

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
