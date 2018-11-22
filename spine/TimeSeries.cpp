#include "TableFeeder.h"

namespace SmartMet
{
namespace Spine
{
namespace TimeSeries
{
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

// TimeSeries corresponds to one column in a table and it is assumed
// that all rows in the same column has same type (or None)
TimeSeriesValueType valueType(const TimeSeries& ts)
{
  std::vector<TimeSeriesValueType> types;
  for (const auto& v : ts)
  {
    TimeSeriesValueType type = valueType(v.value);
    if (type != TimeSeriesValueType::None)
      return type;
  }

  return TimeSeriesValueType::None;
}

TimeSeriesValueType valueType(const Value& val)
{
  if (boost::get<double>(&val) != nullptr)
    return TimeSeriesValueType::Double;
  else if (boost::get<int>(&val) != nullptr)
    return TimeSeriesValueType::Int;
  else if (boost::get<std::string>(&val) != nullptr)
    return TimeSeriesValueType::String;
  else if (boost::get<LonLat>(&val) != nullptr)
    return TimeSeriesValueType::LonLat;
  else if (boost::get<boost::local_time::local_date_time>(&val) != nullptr)
    return TimeSeriesValueType::DateTime;

  return TimeSeriesValueType::None;
}

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet
