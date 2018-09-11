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

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet
