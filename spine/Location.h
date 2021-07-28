// ======================================================================
/*!
 * Location definition, basic geonames data only
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <gis/LandCover.h>

#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace SmartMet
{
namespace Spine
{
using GeoId = int;

class Location
{
 public:
  enum LocationType
  {
    Place,
    Area,
    Path,
    BoundingBox,
    CoordinatePoint,
    Wkt
  };

  GeoId geoid;          // unique numeric ID
  std::string name;     // name of the place
  std::string iso2;     // country isocode
  int municipality;     // municipality or 0
  std::string area;     // municipality or country name
  std::string feature;  // feature code
  std::string country;  // country name, depends on translation, is not const

  boost::optional<int> fmisid;  // station id

  double longitude;                // longitude
  double latitude;                 // latitude
  double radius;                   // radius
  std::string timezone;            // timezone
  int population;                  // population (negative for unknown)
  float elevation;                 // elevation from database
  float dem;                       // elevation from dem
  Fmi::LandCover::Type covertype;  // cover type from global land cover data
  int priority;                    // autocomplete priority
  LocationType type;  // tells in which context location is queried (needed by areaforecast plugin)

  Location(const Location& other) = default;
  Location& operator=(const Location& other) = default;
  Location() = delete;

  Location(GeoId theGeoID,
           const std::string& theName,
           const std::string& theISO2,
           int theMunicipality,
           const std::string& theArea,
           const std::string& theFeature,
           const std::string& theCountry,
           double theLongitude,
           double theLatitude,
           const std::string& theTimeZone,
           int thePopulation = -1,
           float theElevation = std::numeric_limits<float>::quiet_NaN(),
           float theDem = std::numeric_limits<float>::quiet_NaN(),
           Fmi::LandCover::Type theCoverType = Fmi::LandCover::NoData,
           int thePriority = -1)
      : geoid(theGeoID),
        name(theName),
        iso2(theISO2),
        municipality(theMunicipality),
        area(theName == theArea ? "" : theArea)  // prevent name==area
        ,
        feature(theFeature),
        country(theCountry),
        longitude(theLongitude),
        latitude(theLatitude),
        radius(0.0),
        timezone(theTimeZone),
        population(thePopulation),
        elevation(theElevation),
        dem(theDem),
        covertype(theCoverType),
        priority(thePriority),
        type(Place)
  {
  }

  Location(double theLongitude,
           double theLatitude,
           const std::string theName = "",
           const std::string theTimezone = "")
      : geoid(),
        name(theName),
        iso2(),
        municipality(),
        area(),
        feature(),
        country(),
        longitude(theLongitude),
        latitude(theLatitude),
        radius(0.0),
        timezone(theTimezone),
        population(-1),
        elevation(std::numeric_limits<float>::quiet_NaN()),
        dem(std::numeric_limits<float>::quiet_NaN()),
        covertype(Fmi::LandCover::NoData),
        priority(-1),
        type(Place)
  {
  }

  Location(double theLongitude,
           double theLatitude,
           float theElevation,
           Fmi::LandCover::Type theCoverType)
      : geoid(),
        name(),
        iso2(),
        municipality(),
        area(),
        feature(),
        country(),
        longitude(theLongitude),
        latitude(theLatitude),
        radius(0.0),
        timezone(),
        population(-1),
        elevation(theElevation),
        dem(theElevation),
        covertype(theCoverType),
        priority(-1),
        type(Place)
  {
  }

  Location(const std::string& theArea, const double& theRadius)
      : geoid(),
        name(theArea),
        iso2(),
        municipality(),
        area(theArea),
        feature(),
        country(),
        longitude(std::numeric_limits<double>::quiet_NaN()),
        latitude(std::numeric_limits<double>::quiet_NaN()),
        radius(theRadius),
        timezone("localtime"),
        population(-1),
        elevation(std::numeric_limits<float>::quiet_NaN()),
        dem(std::numeric_limits<float>::quiet_NaN()),
        covertype(Fmi::LandCover::NoData),
        priority(-1),
        type(Place)
  {
  }
};  // class Location

using LocationPtr = boost::shared_ptr<const Location>;
using LocationList = std::list<LocationPtr>;

std::string formatLocation(const Location& loc, const std::string& key);
std::string formatLocation(const Location& loc);

// Class to contain location parameter parsing result

// The tag is the "name" given for the location in a query
// and may consist of the name, the geoid or the coordinates
struct TaggedLocation
{
  std::string tag;
  LocationPtr loc;
  TaggedLocation(const std::string& t, LocationPtr& p) : tag(t), loc(p) {}

  TaggedLocation() = delete;
};

using TaggedLocationList = std::list<TaggedLocation>;

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
