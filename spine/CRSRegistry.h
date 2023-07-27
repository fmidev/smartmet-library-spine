#pragma once

#include "ConfigBase.h"
#include "Thread.h"
#include "Value.h"
#include <boost/any.hpp>
#include <boost/array.hpp>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>
#include <macgyver/TypeName.h>
#include <newbase/NFmiPoint.h>
#include <map>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <ostream>
#include <typeinfo>

class OGRSpatialReference;
class OGRGeometry;

namespace SmartMet
{
namespace Spine
{
class CRSRegistry : protected virtual boost::noncopyable
{
  struct MapEntry
  {
    std::string name;
    boost::basic_regex<char> regex;
    boost::shared_ptr<OGRSpatialReference> cs;
    std::map<std::string, boost::any> attrib_map;
    bool swap_coord;

    MapEntry(std::string theName, const boost::optional<std::string>& text);
  };

 public:
  class Transformation : protected virtual boost::noncopyable
  {
   public:
    Transformation();
    virtual ~Transformation();
    virtual std::string get_src_name() const = 0;
    virtual std::string get_dest_name() const = 0;
    virtual NFmiPoint transform(const NFmiPoint& src) = 0;
    virtual boost::array<double, 3> transform(const boost::array<double, 3>& src) = 0;
    virtual void transform(OGRGeometry& geometry) = 0;
  };

  class IdentityTransformation;
  class TransformationImpl;

  CRSRegistry();

  virtual ~CRSRegistry();

  void parse_single_crs_def(Spine::ConfigBase& theConfig, libconfig::Setting& theEntry);

  void read_crs_dir(const boost::filesystem::path& theDir);

  void register_epsg(const std::string& name,
                     int epsg_code,
                     const boost::optional<std::string>& regex = boost::optional<std::string>(),
                     bool swap_coord = false);

  void register_proj4(const std::string& name,
                      const std::string& proj4_def,
                      const boost::optional<std::string>& regex = boost::optional<std::string>(),
                      bool swap_coord = false);

  void register_wkt(const std::string& name,
                    const std::string& wkt_def,
                    const boost::optional<std::string>& regex = boost::optional<std::string>(),
                    bool swap_coord = false);

  boost::shared_ptr<Transformation> create_transformation(const std::string& from,
                                                          const std::string& to);

  SmartMet::Spine::BoundingBox convert_bbox(const SmartMet::Spine::BoundingBox& src,
                                            const std::string& to);

  std::string get_proj4(const std::string& name);

  inline std::size_t size() const { return crs_map.size(); }
  void dump_info(std::ostream& output);

  std::vector<std::string> get_crs_keys() const;

  template <typename Type>
  void set_attribute(const std::string& crs_name, const std::string& attrib_name, const Type& value)
  {
    MapEntry& entry = get_entry(crs_name);
    entry.attrib_map[attrib_name] = value;
  }

  template <typename Type>
  bool get_attribute(const std::string& crs_name, const std::string& attrib_name, Type* value)
  {
    const MapEntry& entry = get_entry(crs_name);
    auto it = entry.attrib_map.find(attrib_name);
    if (it == entry.attrib_map.end())
      return false;

    try
    {
      *value = boost::any_cast<Type>(it->second);
      return true;
    }
    catch (const boost::bad_any_cast&)
    {
      handle_get_attribute_error(crs_name, attrib_name, it->second.type(), typeid(Type));
    }
  }

 private:
  MapEntry& get_entry(const std::string& name);

  void handle_get_attribute_error(const std::string& crs_name,
                                  const std::string& attrib_name,
                                  const std::type_info& actual_type,
                                  const std::type_info& expected_type) __attribute__((noreturn));

  mutable SmartMet::Spine::MutexType rw_lock;
  std::map<std::string, MapEntry> crs_map;
};

}  // namespace Spine
}  // namespace SmartMet
