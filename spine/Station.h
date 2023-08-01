#pragma once

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <list>

namespace SmartMet
{
namespace Spine
{
class Station
{
 public:
  bool isFMIStation = false;
  bool isRoadStation = false;
  bool isMareographStation = false;
  bool isBuoyStation = false;
  bool isSYKEStation = false;
  bool isForeignStation = false;

  std::string station_type;
  std::string distance;
  double station_id = 0;
  double access_policy_id = 0;
  double station_status_id = 0;
  std::string language_code;
  std::string station_formal_name_fi;
  std::string station_formal_name_sv;
  std::string station_formal_name_en;
  boost::posix_time::ptime station_start;
  boost::posix_time::ptime station_end;
  double target_category = 0;
  std::string stationary;
  double latitude_out = 0;
  double longitude_out = 0;
  double station_elevation = 0;
  boost::posix_time::ptime modified_last;
  double modified_by = 0;

  // Possible station identifiers
  int wmo = 0;
  int lpnn = 0;
  int fmisid = 0;
  int rwsid = 0;

  // If the location is requested by finding the nearest observation
  // station for e.g. some latlon coordinates,
  // these fields come in handy when formatting the output
  int geoid = 0;
  std::string requestedName;
  double requestedLon = 0;
  double requestedLat = 0;
  double stationDirection = -1;  // negative number is marker that no direction is calculated
  std::string timezone;
  std::string region;
  std::string country;
  std::string iso2;

  // This will return the requested tag.
  // Because it is request dependent, it will not be serialized
  std::string tag;
  const std::string& station_formal_name(const std::string& language) const;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned /* version */)
  {
    ar& BOOST_SERIALIZATION_NVP(isFMIStation);
    ar& BOOST_SERIALIZATION_NVP(isRoadStation);
    ar& BOOST_SERIALIZATION_NVP(isMareographStation);
    ar& BOOST_SERIALIZATION_NVP(isBuoyStation);
    ar& BOOST_SERIALIZATION_NVP(isSYKEStation);
    ar& BOOST_SERIALIZATION_NVP(isForeignStation);
    ar& BOOST_SERIALIZATION_NVP(station_type);
    ar& BOOST_SERIALIZATION_NVP(distance);
    ar& BOOST_SERIALIZATION_NVP(station_id);
    ar& BOOST_SERIALIZATION_NVP(access_policy_id);
    ar& BOOST_SERIALIZATION_NVP(station_status_id);
    ar& BOOST_SERIALIZATION_NVP(language_code);
    ar& BOOST_SERIALIZATION_NVP(station_formal_name_fi);
    ar& BOOST_SERIALIZATION_NVP(station_formal_name_sv);
    ar& BOOST_SERIALIZATION_NVP(station_formal_name_en);
    ar& BOOST_SERIALIZATION_NVP(station_start);
    ar& BOOST_SERIALIZATION_NVP(station_end);
    ar& BOOST_SERIALIZATION_NVP(target_category);
    ar& BOOST_SERIALIZATION_NVP(stationary);
    ar& BOOST_SERIALIZATION_NVP(latitude_out);
    ar& BOOST_SERIALIZATION_NVP(longitude_out);
    ar& BOOST_SERIALIZATION_NVP(station_elevation);
    ar& BOOST_SERIALIZATION_NVP(modified_last);
    ar& BOOST_SERIALIZATION_NVP(modified_by);
    ar& BOOST_SERIALIZATION_NVP(wmo);
    ar& BOOST_SERIALIZATION_NVP(lpnn);
    ar& BOOST_SERIALIZATION_NVP(fmisid);
    ar& BOOST_SERIALIZATION_NVP(rwsid);
    ar& BOOST_SERIALIZATION_NVP(geoid);
    ar& BOOST_SERIALIZATION_NVP(requestedName);
    ar& BOOST_SERIALIZATION_NVP(requestedLon);
    ar& BOOST_SERIALIZATION_NVP(requestedLat);
    ar& BOOST_SERIALIZATION_NVP(stationDirection);
    ar& BOOST_SERIALIZATION_NVP(timezone);
    ar& BOOST_SERIALIZATION_NVP(region);
    ar& BOOST_SERIALIZATION_NVP(country);
    ar& BOOST_SERIALIZATION_NVP(iso2);
  }

  std::string hash() const;

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
