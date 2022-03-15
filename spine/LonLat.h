#pragma once

namespace SmartMet
{
namespace Spine
{

enum class LonLatFormat { LATLON, LONLAT };

struct LonLat
{
  LonLat(double longitude, double latitude) : lon(longitude), lat(latitude) {}
  double lon;
  double lat;
};

bool operator==(const LonLat& lonlat1, const LonLat& lonlat2)
{
  return (lonlat1.lon == lonlat2.lon && lonlat1.lat == lonlat2.lat);
}

}
}
