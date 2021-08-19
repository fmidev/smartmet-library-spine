#include "TableFeeder.h"
#include <boost/make_shared.hpp>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Spine
{
namespace TimeSeries
{
  
std::ostream& operator<<(std::ostream& os, const LocalTimePool& localTimePool)
{
  localTimePool.print(os);
  return os;
}

const boost::local_time::local_date_time&  LocalTimePool::create(const boost::posix_time::ptime& t, const boost::local_time::time_zone_ptr& tz)
{
  std::string key =  (Fmi::to_iso_string(t) + tz->to_posix_string());

  if(localtimes.find(key) == localtimes.end())
    localtimes.insert(std::make_pair(key, boost::local_time::local_date_time(t, tz)));

  return localtimes.at(key);
}

size_t LocalTimePool::size() const 
{
  return localtimes.size();
}

void LocalTimePool::print(std::ostream& os) const
{
  for(const auto& item : localtimes)
	os << item.first << " -> " << item.second << std::endl;
}

TimeSeries::TimeSeries(LocalTimePoolPtr time_pool)
{
  local_time_pool = time_pool;
}
  
void TimeSeries::emplace_back(const TimedValue& tv)
{
  TimedValueVector::emplace_back(TimedValue(local_time_pool->create(tv.time.utc_time(), tv.time.zone()),tv.value));
}
  
void TimeSeries::push_back(const TimedValue& tv)
{
  TimedValueVector::push_back(TimedValue(local_time_pool->create(tv.time.utc_time(), tv.time.zone()),tv.value));
}

TimedValueVector::iterator TimeSeries::insert(TimedValueVector::iterator pos, const TimedValue& tv)
{
  return TimedValueVector::insert(pos, TimedValue(local_time_pool->create(tv.time.utc_time(), tv.time.zone()),tv.value));
}

void TimeSeries::insert(TimedValueVector::iterator pos, TimedValueVector::iterator first, TimedValueVector::iterator last)
{
  TimedValueVector::insert(pos, first, last);
}

void TimeSeries::insert(TimedValueVector::iterator pos, TimedValueVector::const_iterator first, TimedValueVector::const_iterator last)
{
  TimedValueVector::insert(pos, first, last);
}

TimeSeries& TimeSeries::operator=(const TimeSeries& ts) 
{
  clear();
  TimedValueVector::insert(end(), ts.begin(), ts.end());  
  local_time_pool = ts.local_time_pool;

  return *this; 
}

bool operator==(const LonLat& lonlat1, const LonLat& lonlat2)
{
  return (lonlat1.lon == lonlat2.lon && lonlat1.lat == lonlat2.lat);
}

template <class T>
bool None::operator==(const T& /* other */) const
{
  return false;
}

template <>
bool None::operator==(const None& /* other */) const
{
  return true;
}

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet
