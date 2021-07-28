#include "CRSRegistry.h"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/spirit/include/qi.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <ogr_srs_api.h>
#include <stdexcept>

namespace ba = boost::algorithm;
namespace qi = boost::spirit::qi;
namespace ac = boost::spirit::ascii;
namespace fs = boost::filesystem;

#if GDAL_VERSION_MAJOR >= 3
#define OGRFree(x) CPLFree(x)
#endif

namespace SmartMet
{
namespace Spine
{
/**
 *   @brief [INTERNAL] Identity CRS transformation
 */
class CRSRegistry::IdentityTransformation : public CRSRegistry::Transformation
{
  const std::string crs_name;

 public:
  IdentityTransformation(const std::string& crs_name);

  virtual ~IdentityTransformation() = default;

  virtual std::string get_src_name() const;

  virtual std::string get_dest_name() const;

  virtual NFmiPoint transform(const NFmiPoint& src);

  virtual boost::array<double, 3> transform(const boost::array<double, 3>& src);

  virtual void transform(OGRGeometry& geometry);
};

/**
 *   @brief [INTERNAL] CRS transformation implementation
 */
class CRSRegistry::TransformationImpl : public CRSRegistry::Transformation
{
  const std::string from_name;
  const std::string to_name;
  bool swap1, swap2;
  OGRCoordinateTransformation* conv;

 public:
  TransformationImpl(const CRSRegistry::MapEntry& from, const CRSRegistry::MapEntry& to);

  virtual ~TransformationImpl();

  virtual std::string get_src_name() const;

  virtual std::string get_dest_name() const;

  virtual NFmiPoint transform(const NFmiPoint& src);

  virtual boost::array<double, 3> transform(const boost::array<double, 3>& src);

  virtual void transform(OGRGeometry& geometry);
};

#define CHECK_NAME(name)   \
  if (crs_map.count(name)) \
    throw Fmi::Exception(BCP, "Duplicate name of coordinate system (" + (name) + ")!");

CRSRegistry::CRSRegistry() {}

CRSRegistry::~CRSRegistry() {}

void CRSRegistry::register_epsg(const std::string& name,
                                int epsg_code,
                                boost::optional<std::string> regex,
                                bool swap_coord)
{
  try
  {
    Spine::WriteLock lock(rw_lock);
    const std::string nm = Fmi::ascii_tolower_copy(name);
    CHECK_NAME(nm)

    MapEntry entry(name, regex);
    if (entry.cs->importFromEPSG(epsg_code) != OGRERR_NONE)
    {
      throw Fmi::Exception(BCP, "Failed to register projection EPSG:" + Fmi::to_string(epsg_code));
    }
#if GDAL_VERSION_MAJOR >= 3
    entry.cs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
#endif

    entry.swap_coord = swap_coord;

    entry.attrib_map["epsg"] = epsg_code;
    entry.attrib_map["swapCoord"] = swap_coord;

    crs_map.insert(std::make_pair(nm, entry));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CRSRegistry::register_proj4(const std::string& name,
                                 const std::string& proj4_def,
                                 boost::optional<std::string> regex,
                                 bool swap_coord)
{
  try
  {
    Spine::WriteLock lock(rw_lock);

    const std::string nm = Fmi::ascii_tolower_copy(name);
    CHECK_NAME(nm)

    MapEntry entry(name, regex);
    if (entry.cs->importFromProj4(proj4_def.c_str()) != OGRERR_NONE)
    {
      throw Fmi::Exception(BCP, "Failed to parse PROJ.4 definition '" + proj4_def + "'!");
    }
    entry.cs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

#if GDAL_VERSION_MAJOR >= 3
    entry.cs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
#endif

    entry.swap_coord = swap_coord;

    entry.attrib_map["swapCoord"] = swap_coord;

    crs_map.insert(std::make_pair(nm, entry));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CRSRegistry::register_wkt(const std::string& name,
                               const std::string& wkt_def,
                               boost::optional<std::string> regex,
                               bool swap_coord)
{
  try
  {
    Spine::WriteLock lock(rw_lock);

    const std::string nm = Fmi::ascii_tolower_copy(name);
    CHECK_NAME(nm)

    MapEntry entry(name, regex);

    const auto* str = wkt_def.c_str();

    int ret = entry.cs->importFromWkt(&str);

    if (ret == OGRERR_NONE)
    {
      // Check whether there is some garbage after WKT
      for (; *str and std::isspace(*str); str++)
      {
      }

      if (*str == 0)
      {
        crs_map.insert(std::make_pair(nm, entry));
        return;
      }
    }

    entry.cs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    entry.attrib_map["swapCoord"] = swap_coord;

    entry.swap_coord = swap_coord;

    throw Fmi::Exception(BCP, "Failed to parse PROJ.4 definition '" + wkt_def + "'!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<CRSRegistry::Transformation> CRSRegistry::create_transformation(
    const std::string& from, const std::string& to)
{
  try
  {
    const MapEntry& e_from = get_entry(from);
    const MapEntry& e_to = get_entry(to);
    boost::shared_ptr<Transformation> result;

    if (&e_from == &e_to)
    {
      result.reset(new IdentityTransformation(e_from.name));
    }
    else
    {
      result.reset(new TransformationImpl(e_from, e_to));
    }

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::BoundingBox CRSRegistry::convert_bbox(const Spine::BoundingBox& src, const std::string& to)
{
  try
  {
    const MapEntry& e_from = get_entry(src.crs);
    const MapEntry& e_to = get_entry(to);

    if (&e_from == &e_to)
    {
      return src;
    }

    TransformationImpl transformation(e_from, e_to);

    const int NPOINTS = 10;
    OGRLinearRing boundary;

    double xStep = (src.xMax - src.xMin) / (NPOINTS - 1);
    double yStep = (src.yMax - src.yMin) / (NPOINTS - 1);

    for (int i = 0; i < NPOINTS; i++)
      boundary.addPoint(src.xMin, src.yMin + i * yStep);
    for (int i = 1; i < NPOINTS; i++)
      boundary.addPoint(src.xMin + i * xStep, src.yMax);
    for (int i = 1; i < NPOINTS; i++)
      boundary.addPoint(src.xMax, src.yMax - i * yStep);
    for (int i = 1; i < NPOINTS; i++)
      boundary.addPoint(src.xMax - i * xStep, src.yMin);

    boundary.closeRings();

    OGRPolygon polygon;
    polygon.addRing(&boundary);

    transformation.transform(polygon);

    OGREnvelope envelope;
    polygon.getEnvelope(&envelope);

    Spine::BoundingBox result;
    result.xMin = envelope.MinX;
    result.yMin = envelope.MinY;
    result.xMax = envelope.MaxX;
    result.yMax = envelope.MaxY;
    result.crs = to;

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string CRSRegistry::get_proj4(const std::string& name)
{
  try
  {
    char* tmp = nullptr;
    auto& entry = get_entry(name);
    entry.cs->exportToProj4(&tmp);
    std::string result(tmp);
    CPLFree(tmp);
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CRSRegistry::dump_info(std::ostream& output)
{
  try
  {
    for (const auto& item : crs_map)
    {
      output << "CRSRegistry: name='" << item.second.name << "' regex='" << item.second.regex
             << "' proj4='" << get_proj4(item.second.name) << "'" << std::endl;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<std::string> CRSRegistry::get_crs_keys() const
{
  try
  {
    std::vector<std::string> result;
    for (const auto& item : crs_map)
    {
      result.push_back(item.first);
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

CRSRegistry::MapEntry& CRSRegistry::get_entry(const std::string& name)
{
  try
  {
    Spine::ReadLock lock(rw_lock);
    const std::string name_lower = Fmi::ascii_tolower_copy(name);
    auto it = crs_map.find(name_lower);

    if (it != crs_map.end())
    {
      // Found entry by name ==> return it
      return it->second;
    }

    const std::string opengisPrefix = "http://www.opengis.net/def/crs/epsg/";
    bool isOpengis =
        qi::phrase_parse(name_lower.begin(),
                         name_lower.end(),
                         qi::string(opengisPrefix) >> qi::ushort_ >> '/' >> qi::ushort_ >> qi::eps,
                         ac::space);
    std::string epsgCode = "EPSG::";

    if (isOpengis)
    {
      std::string::size_type pos = name_lower.find_last_of('/');
      if (pos != std::string::npos)
      {
        epsgCode.append(name_lower.substr(pos + 1));
      }
      else
        isOpengis = false;
    }

    // Not found by name: try searching using regex match
    for (auto& item : crs_map)
    {
      if (not item.second.regex.empty())
      {
        if (isOpengis and boost::regex_match(epsgCode, item.second.regex))
          return item.second;

        if (boost::regex_match(name, item.second.regex))
          return item.second;
      }
    }

    // Still not found ==> throw an error
    throw Fmi::Exception(BCP, "Coordinate system '" + name + "' not found!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CRSRegistry::handle_get_attribute_error(const std::string& crs_name,
                                             const std::string& attrib_name,
                                             const std::type_info& actual_type,
                                             const std::type_info& expected_type)
{
  try
  {
    throw Fmi::Exception(
        BCP, "Type mismatch of attribute '" + attrib_name + "' for CRS '" + crs_name + "'!")
        .addParameter("Expected", Fmi::demangle_cpp_type_name(expected_type.name()))
        .addParameter("Found", Fmi::demangle_cpp_type_name(actual_type.name()));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

CRSRegistry::Transformation::Transformation() {}

CRSRegistry::Transformation::~Transformation() {}

CRSRegistry::MapEntry::MapEntry(const std::string& theName, boost::optional<std::string> text)
    : name(theName), cs(new OGRSpatialReference)
{
  try
  {
    if (text)
    {
      try
      {
        regex.assign(*text, boost::regex_constants::perl | boost::regex_constants::icase);
      }
      catch (...)
      {
        std::cerr << METHOD_NAME << ": failed to parse PERL regular expression '" << *text << "'"
                  << std::endl;

        Fmi::Exception exception(BCP, "Failed to parse PERL regular expression");
        if (text)
          exception.addParameter("text", *text);
        throw exception;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

CRSRegistry::IdentityTransformation::IdentityTransformation(const std::string& theCrsName)
    : crs_name(theCrsName)
{
}

std::string CRSRegistry::IdentityTransformation::get_src_name() const
{
  return crs_name;
}

std::string CRSRegistry::IdentityTransformation::get_dest_name() const
{
  return crs_name;
}

NFmiPoint CRSRegistry::IdentityTransformation::transform(const NFmiPoint& src)
{
  return src;
}

boost::array<double, 3> CRSRegistry::IdentityTransformation::transform(
    const boost::array<double, 3>& src)
{
  return src;
}

void CRSRegistry::IdentityTransformation::transform(OGRGeometry& geometry)
{
  (void)geometry;
}

CRSRegistry::TransformationImpl::TransformationImpl(const CRSRegistry::MapEntry& from,
                                                    const CRSRegistry::MapEntry& to)
    : from_name(from.name),
      to_name(to.name),
      swap1(from.swap_coord),
      swap2(to.swap_coord),
      conv(OGRCreateCoordinateTransformation(from.cs.get(), to.cs.get()))
{
  try
  {
    if (not conv)
    {
      throw Fmi::Exception(
          BCP,
          "Failed to create coordinate transformation '" + from_name + "' to '" + to_name + "'!");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

CRSRegistry::TransformationImpl::~TransformationImpl()
{
  if (conv)
    OGRCoordinateTransformation::DestroyCT(conv);
}

std::string CRSRegistry::TransformationImpl::get_src_name() const
{
  return from_name;
}

std::string CRSRegistry::TransformationImpl::get_dest_name() const
{
  return to_name;
}

NFmiPoint CRSRegistry::TransformationImpl::transform(const NFmiPoint& src)
{
  try
  {
    double x = swap1 ? src.Y() : src.X();
    double y = swap1 ? src.X() : src.Y();
    if (conv->Transform(1, &x, &y, nullptr))
    {
      NFmiPoint result(swap2 ? y : x, swap2 ? x : y);
      return result;
    }

    std::ostringstream msg;
    msg << "Coordinate transformatiom from " << from_name << " to " << to_name << " failed for ("
        << src.X() << ", " << src.Y() << ")";
    throw Fmi::Exception(BCP, msg.str());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::array<double, 3> CRSRegistry::TransformationImpl::transform(
    const boost::array<double, 3>& src)
{
  try
  {
    double x = swap1 ? src[1] : src[0];
    double y = swap1 ? src[0] : src[1];
    double z = src[2];
    if (conv->Transform(1, &x, &y, &z))
    {
      boost::array<double, 3> result;
      result[0] = swap2 ? y : x;
      result[1] = swap2 ? x : y;
      result[2] = z;
      return result;
    }

    std::ostringstream msg;
    msg << "Coordinate transformatiom from " << from_name << " to " << to_name << " failed for ("
        << src[0] << ", " << src[1] << ", " << src[1] << ")";
    throw Fmi::Exception(BCP, msg.str());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CRSRegistry::TransformationImpl::transform(OGRGeometry& geometry)
{
  try
  {
    OGRErr err = geometry.transform(conv);
    if (err != OGRERR_NONE)
    {
      std::ostringstream msg;
      char* gText = nullptr;
      geometry.exportToWkt(&gText);
      msg << "Failed to transform geometry " << gText << " to " << to_name;
      CPLFree(gText);
      throw Fmi::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CRSRegistry::parse_single_crs_def(Spine::ConfigBase& theConfig, libconfig::Setting& theEntry)
{
  try
  {
    theConfig.assert_is_group(theEntry);

    const auto name = theConfig.get_mandatory_config_param<std::string>(theEntry, "name");
    bool swap_coord = theConfig.get_optional_config_param<bool>(theEntry, "swapCoord", false);
    bool show_height = theConfig.get_optional_config_param<bool>(theEntry, "showHeight", false);
    const auto axis_labels =
        theConfig.get_mandatory_config_param<std::string>(theEntry, "axisLabels");
    const auto proj_epoch_uri =
        theConfig.get_mandatory_config_param<std::string>(theEntry, "projEpochUri");
    std::string proj_uri;

    if (theEntry.exists("epsg"))
    {
      using boost::format;
      int epsg = theConfig.get_mandatory_config_param<int>(theEntry, "epsg");
      const std::string def_regex = str(format("(?:urn:ogc:def:crs:|)EPSG:{1,2}%04u") % epsg);
      const std::string def_proj_uri =
          str(format("http://www.opengis.net/def/crs/EPSG/0/%04u") % epsg);
      auto regex = theConfig.get_optional_config_param<std::string>(theEntry, "regex", def_regex);
      proj_uri =
          theConfig.get_optional_config_param<std::string>(theEntry, "projUri", def_proj_uri);
      register_epsg(name, epsg, regex, swap_coord);
    }
    else
    {
      const auto proj4 = theConfig.get_mandatory_config_param<std::string>(theEntry, "proj4");
      const auto regex = theConfig.get_mandatory_config_param<std::string>(theEntry, "regex");
      proj_uri = theConfig.get_mandatory_config_param<std::string>(theEntry, "projUri");
      register_proj4(name, proj4, regex, swap_coord);
      if (theEntry.exists("epsgCode"))
      {
        int epsg = theConfig.get_mandatory_config_param<int>(theEntry, "epsgCode");
        set_attribute(name, "epsg", epsg);
      }
    }

    set_attribute(name, "showHeight", show_height);
    set_attribute(name, "projUri", proj_uri);
    set_attribute(name, "projEpochUri", proj_epoch_uri);
    set_attribute(name, "axisLabels", axis_labels);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CRSRegistry::read_crs_dir(const fs::path& theDir)
{
  try
  {
    if (not fs::exists(theDir))
      throw Fmi::Exception(BCP, "Gis config: CRS directory '" + theDir.string() + "' not found");

    if (not fs::is_directory(theDir))
      throw Fmi::Exception(
          BCP, "Gis config: CRS directory '" + theDir.string() + "' is not a directory");

    for (auto it = fs::directory_iterator(theDir); it != fs::directory_iterator(); ++it)
    {
      const fs::path entry = *it;
      const auto fn = entry.filename().string();
      if (fs::is_regular_file(entry) and not ba::starts_with(fn, ".") and
          not ba::starts_with(fn, "#") and ba::ends_with(fn, ".conf"))
      {
        boost::shared_ptr<Spine::ConfigBase> desc(
            new Spine::ConfigBase(entry.string(), "CRS description"));

        try
        {
          auto& root = desc->get_root();
          parse_single_crs_def(*desc, root);
        }
        catch (...)
        {
          throw Fmi::Exception::Trace(BCP, "Invalid CRS description!")
              .addParameter("File", entry.string());
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
