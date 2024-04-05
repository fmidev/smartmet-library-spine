#pragma once

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <macgyver/DateTime.h>
#include <list>

namespace SmartMet
{
namespace Spine
{
class Station
{
 public:
  std::string type;
  std::string language_code;
  std::string formal_name_fi;
  std::string formal_name_sv;
  std::string formal_name_en;
  Fmi::DateTime station_start;
  Fmi::DateTime station_end;
  double latitude = 0;
  double longitude = 0;
  double elevation = 0;
  bool isFmi = false;
  bool isForeign = false;
  bool isStationary = true;
  bool isMareograph = false;
  bool isBuoy = false;
  bool isRoad = false;
  bool isSyke = false;
  Fmi::DateTime modified_last;

  // Possible station identifiers
  std::string wsi;  // WIGOS Station Identifier
  int fmisid = 0;   // FMI station number
  int wmo = 0;      // WMO station number, deprecated
  int lpnn = 0;     // Legacy FMI station number
  int rwsid = 0;    // Road station number
  int geoid = 0;    // Geonames ID
  int modified_by = 0;

  // If the location is requested by finding the nearest observation
  // station for e.g. some latlon coordinates,
  // these fields come in handy when formatting the output
  std::string requestedName;
  std::string distance;
  std::string timezone;
  std::string region;
  std::string country;
  std::string iso2;
  double requestedLon = 0;
  double requestedLat = 0;
  double stationDirection = -1;  // negative number is marker that no direction is calculated

  // This will return the requested tag.
  // Because it is request dependent, it will not be serialized
  std::string tag;
  const std::string& station_formal_name(const std::string& language) const;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned /* version */)
  {
    ar& BOOST_SERIALIZATION_NVP(wsi);
    ar& BOOST_SERIALIZATION_NVP(fmisid);
    ar& BOOST_SERIALIZATION_NVP(rwsid);
    ar& BOOST_SERIALIZATION_NVP(geoid);
    ar& BOOST_SERIALIZATION_NVP(wmo);
    ar& BOOST_SERIALIZATION_NVP(lpnn);
    ar& BOOST_SERIALIZATION_NVP(type);
    ar& BOOST_SERIALIZATION_NVP(language_code);
    ar& BOOST_SERIALIZATION_NVP(formal_name_fi);
    ar& BOOST_SERIALIZATION_NVP(formal_name_sv);
    ar& BOOST_SERIALIZATION_NVP(formal_name_en);
    ar& BOOST_SERIALIZATION_NVP(station_start);
    ar& BOOST_SERIALIZATION_NVP(station_end);
    ar& BOOST_SERIALIZATION_NVP(latitude);
    ar& BOOST_SERIALIZATION_NVP(longitude);
    ar& BOOST_SERIALIZATION_NVP(elevation);
    ar& BOOST_SERIALIZATION_NVP(isStationary);
    ar& BOOST_SERIALIZATION_NVP(isFmi);
    ar& BOOST_SERIALIZATION_NVP(isForeign);
    ar& BOOST_SERIALIZATION_NVP(isMareograph);
    ar& BOOST_SERIALIZATION_NVP(isBuoy);
    ar& BOOST_SERIALIZATION_NVP(isRoad);
    ar& BOOST_SERIALIZATION_NVP(isSyke);
    ar& BOOST_SERIALIZATION_NVP(timezone);
    ar& BOOST_SERIALIZATION_NVP(region);
    ar& BOOST_SERIALIZATION_NVP(country);
    ar& BOOST_SERIALIZATION_NVP(iso2);
    ar& BOOST_SERIALIZATION_NVP(modified_by);
    ar& BOOST_SERIALIZATION_NVP(modified_last);
  }

  std::size_t hash() const;

};  // class Station

using Stations = std::vector<Station>;

struct TaggedFMISID
{
  std::string tag;
  int fmisid{-1};
  double direction{-1};
  std::string distance;

  TaggedFMISID() = delete;
  TaggedFMISID(std::string t, int sid, double dir = 0.0, std::string dis = "")
      : tag(std::move(t)), fmisid(sid), direction(dir), distance(std::move(dis))
  {
  }
};

using TaggedFMISIDList = std::list<TaggedFMISID>;

}  // namespace Spine
}  // namespace SmartMet
