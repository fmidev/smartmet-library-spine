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

namespace
{
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

const std::string nanstring = "nan";

// The value as a string, or "nan" for NaN values
template <typename T>
std::string nan_or_string(T value)
{
  return (std::isnan(value) ? nanstring : Fmi::to_string(value));
}

// Format a numeric location parameter, false if the key is not a numeric parameter
bool format_numeric_location(const Location& loc, const std::string& key, std::string& result)
{
  if (key == "latitude")
    result = nan_or_string(loc.latitude);
  else if (key == "longitude")
    result = nan_or_string(loc.longitude);
  else if (key == "geoid")
    result = Fmi::to_string(loc.geoid);
  else if (key == "municipality")
    result = Fmi::to_string(loc.municipality);
  else if (key == "population")
    result = nan_or_string(loc.population);
  else if (key == "elevation")
    result = nan_or_string(loc.elevation);
  else if (key == "dem")
    result = nan_or_string(loc.dem);
  else if (key == "covertype")
    result = Fmi::to_string(static_cast<int>(loc.covertype));
  else if (key == "priority")
    result = nan_or_string(loc.priority);
  else
    return false;
  return true;
}
}  // namespace

std::string formatLocation(const Location& loc, const std::string& key)
{
  try
  {
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
    std::string result;
    if (format_numeric_location(loc, key, result))
      return result;
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
