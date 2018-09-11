#pragma once

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace Spine
{
class Station
{
 public:
  bool isFMIStation;
  bool isRoadStation;
  bool isMareographStation;
  bool isBuoyStation;
  bool isSYKEStation;
  bool isForeignStation;

  std::string station_type;
  std::string distance;
  double station_id;
  double access_policy_id;
  double station_status_id;
  std::string language_code;
  std::string station_formal_name;
  std::string station_system_name;
  std::string station_global_name;
  std::string station_global_code;
  boost::posix_time::ptime station_start;
  boost::posix_time::ptime station_end;
  double target_category;
  std::string stationary;
  double latitude_out;
  double longitude_out;
  double station_elevation;
  boost::posix_time::ptime modified_last;
  double modified_by;

  // Possible station identifiers
  int wmo;
  int lpnn;
  int fmisid;
  int rwsid;

  // If the location is requested by finding the nearest observation
  // station for e.g. some latlon coordinates,
  // these fields come in handy when formatting the output
  int geoid;
  std::string requestedName;
  double requestedLon;
  double requestedLat;
  double stationDirection;
  std::string timezone;
  std::string region;
  std::string country;
  std::string iso2;

  // This will return the requested tag.
  // Because it is request dependent, it will not be serialized
  std::string tag;

  // Initialize the station indicators to false by default
  Station();

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
    ar& BOOST_SERIALIZATION_NVP(station_formal_name);
    ar& BOOST_SERIALIZATION_NVP(station_system_name);
    ar& BOOST_SERIALIZATION_NVP(station_global_name);
    ar& BOOST_SERIALIZATION_NVP(station_global_code);
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

}  // namespace Spine
}  // namespace SmartMet
