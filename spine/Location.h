// ======================================================================
/*!
 * Location definition, basic geonames data only
 */
// ======================================================================

#pragma once

#include <optional>
#include <memory>
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

  GeoId geoid = 0;       // unique numeric ID
  std::string name;      // name of the place
  std::string iso2;      // country isocode
  int municipality = 0;  // municipality or 0
  std::string area;      // municipality or country name
  std::string feature;   // feature code
  std::string country;   // country name, depends on translation, is not const

  std::optional<int> fmisid;  // station id

  double longitude = std::numeric_limits<double>::quiet_NaN();  // longitude
  double latitude = std::numeric_limits<double>::quiet_NaN();   // latitude
  double radius = 0;                                            // radius
  std::string timezone;                                         // timezone
  int population = -1;                                          // population (negative for unknown)
  float elevation = std::numeric_limits<float>::quiet_NaN();    // elevation from database
  float dem = std::numeric_limits<float>::quiet_NaN();          // elevation from dem
  Fmi::LandCover::Type covertype = Fmi::LandCover::NoData;      // from global land cover data
  int priority = -1;                                            // autocomplete priority
  LocationType type;  // tells in which context location is queried (needed by areaforecast plugin)

  Location(const Location& other) = default;
  Location& operator=(const Location& other) = default;
  Location() = delete;

  Location(GeoId theGeoID,
           std::string theName,
           std::string theISO2,
           int theMunicipality,
           std::string theArea,
           std::string theFeature,
           std::string theCountry,
           double theLongitude,
           double theLatitude,
           std::string theTimeZone,
           int thePopulation = -1,
           float theElevation = std::numeric_limits<float>::quiet_NaN(),
           float theDem = std::numeric_limits<float>::quiet_NaN(),
           Fmi::LandCover::Type theCoverType = Fmi::LandCover::NoData,
           int thePriority = -1)
      : geoid(theGeoID),
        name(std::move(theName)),
        iso2(std::move(theISO2)),
        municipality(theMunicipality),
        area(name == theArea ? "" : std::move(theArea)),  // prevent name==area
        feature(std::move(theFeature)),
        country(std::move(theCountry)),
        longitude(theLongitude),
        latitude(theLatitude),
        timezone(std::move(theTimeZone)),
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
           std::string theName = "",
           std::string theTimezone = "")
      : name(std::move(theName)),
        longitude(theLongitude),
        latitude(theLatitude),
        timezone(std::move(theTimezone)),
        type(Place)
  {
  }

  Location(double theLongitude,
           double theLatitude,
           float theElevation,
           Fmi::LandCover::Type theCoverType)
      : longitude(theLongitude),
        latitude(theLatitude),
        elevation(theElevation),
        dem(theElevation),
        covertype(theCoverType),
        type(Place)
  {
  }

  Location(const std::string& theArea, double theRadius)
      : name(theArea), area(theArea), radius(theRadius), timezone("localtime"), type(Place)
  {
  }
};  // class Location

using LocationPtr = std::shared_ptr<const Location>;
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
  TaggedLocation(std::string t, const LocationPtr& p) : tag(std::move(t)), loc(p) {}

  TaggedLocation() = delete;
};

using TaggedLocationList = std::list<TaggedLocation>;

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
