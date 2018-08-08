#include "Location.h"
#include "Exception.h"
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
#ifndef UNREACHABLE
      default:
        return "";
#endif
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception(BCP, "Unsupported location parameter name '" + key + "'!");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string formatLocation(const Location& loc)
{
  try
  {
    std::stringstream ss;

    ss << "geoid:        " << formatLocation(loc, "geoid") << std::endl;
    ss << "name:         " << formatLocation(loc, "name") << std::endl;
    ss << "iso2:         " << formatLocation(loc, "iso2") << std::endl;
    ss << "municipality: " << formatLocation(loc, "municipality") << std::endl;
    ss << "region:       " << formatLocation(loc, "region") << std::endl;
    ss << "feature:      " << formatLocation(loc, "feature") << std::endl;
    ss << "country:      " << formatLocation(loc, "country") << std::endl;
    ss << "longitude:    " << formatLocation(loc, "longitude") << std::endl;
    ss << "latitude:     " << formatLocation(loc, "latitude") << std::endl;
    ss << "radius:       " << loc.radius << std::endl;
    ss << "timezone:     " << formatLocation(loc, "tz") << std::endl;
    ss << "population:   " << formatLocation(loc, "population") << std::endl;
    ss << "elevation:    " << formatLocation(loc, "elevation") << std::endl;
    ss << "dem:          " << formatLocation(loc, "dem") << std::endl;
    ss << "covertype:    " << formatLocation(loc, "covertype") << std::endl;
    ss << "priority:     " << formatLocation(loc, "priority") << std::endl;
    ss << "type:         " << formatLocation(loc, "type") << std::endl;

    return ss.str();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
