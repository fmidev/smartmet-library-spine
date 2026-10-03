#include "Location.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <cmath>
#include <sstream>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------

std::string type_string(const Location::LocationType& type)
{
  try
  {
    switch (type)
    {
      case Location::Place:
        return "Place";
      case Location::Area:
        return "Area";
      case Location::Path:
        return "Path";
      case Location::BoundingBox:
        return "BoundingBox";
      case Location::Wkt:
        return "Wkt";
      case Location::CoordinatePoint:
        return "CoordinatePoint";
#ifdef __GNUC__
      default:
        return "";
#endif
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string formatLocation(const Location& loc, const std::string& key)
{
  try
  {
    static std::string nanstring = "nan";

    if (key == "name")
      return loc.name;
    if (key == "iso2")
      return loc.iso2;
    if (key == "region")
    {
      // Region may be empty, if it is return name instead
      if (loc.area.empty())
        return (loc.name.empty() ? nanstring : loc.name);
      return loc.area;
    }
    if (key == "feature")
      return loc.feature;
    if (key == "country")
      return loc.country;
    if (key == "tz")
      return loc.timezone;
    if (key == "latitude")
      return (std::isnan(loc.latitude) ? nanstring : Fmi::to_string(loc.latitude));
    if (key == "longitude")
      return (std::isnan(loc.longitude) ? nanstring : Fmi::to_string(loc.longitude));
    if (key == "geoid")
      return Fmi::to_string(loc.geoid);
    if (key == "municipality")
      return Fmi::to_string(loc.municipality);
    if (key == "population")
      return (std::isnan(loc.population) ? nanstring : Fmi::to_string(loc.population));
    if (key == "elevation")
      return (std::isnan(loc.elevation) ? nanstring : Fmi::to_string(loc.elevation));
    if (key == "dem")
      return (std::isnan(loc.dem) ? nanstring : Fmi::to_string(loc.dem));
    if (key == "covertype")
      return Fmi::to_string(static_cast<int>(loc.covertype));
    if (key == "priority")
      return (std::isnan(loc.priority) ? nanstring : Fmi::to_string(loc.priority));
    if (key == "type")
      return type_string(loc.type);
    if (key == "fmisid")
    {
      if (loc.fmisid)
        return Fmi::to_string(*loc.fmisid);
      return "-";
    }
    throw Fmi::Exception(BCP, "Unsupported location parameter name '" + key + "'!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string formatLocation(const Location& loc)
{
  try
  {
    std::stringstream ss;

    ss << "geoid:        " << formatLocation(loc, "geoid") << '\n';
    ss << "name:         " << formatLocation(loc, "name") << '\n';
    ss << "iso2:         " << formatLocation(loc, "iso2") << '\n';
    ss << "fmisid:       " << formatLocation(loc, "fmisid") << '\n';
    ss << "municipality: " << formatLocation(loc, "municipality") << '\n';
    ss << "region:       " << formatLocation(loc, "region") << '\n';
    ss << "feature:      " << formatLocation(loc, "feature") << '\n';
    ss << "country:      " << formatLocation(loc, "country") << '\n';
    ss << "longitude:    " << formatLocation(loc, "longitude") << '\n';
    ss << "latitude:     " << formatLocation(loc, "latitude") << '\n';
    ss << "radius:       " << loc.radius << '\n';
    ss << "timezone:     " << formatLocation(loc, "tz") << '\n';
    ss << "population:   " << formatLocation(loc, "population") << '\n';
    ss << "elevation:    " << formatLocation(loc, "elevation") << '\n';
    ss << "dem:          " << formatLocation(loc, "dem") << '\n';
    ss << "covertype:    " << formatLocation(loc, "covertype") << '\n';
    ss << "priority:     " << formatLocation(loc, "priority") << '\n';
    ss << "type:         " << formatLocation(loc, "type") << '\n';

    return ss.str();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
