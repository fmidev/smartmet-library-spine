#include "Basemap.h"
#include <fmt/format.h>
#include <gdal_priv.h>
#include <macgyver/Exception.h>
#include <libconfig.h++>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>
#include <cmath>
#include <memory>
#include <vector>

namespace SmartMet
{
namespace Spine
{
namespace OpenAPI
{
namespace
{
BasemapLayerSource parseLayer(const libconfig::Setting& group, const char* name)
{
  BasemapLayerSource layer;
  if (!group.exists(name))
    return layer;
  const auto& setting = group[name];
  if (!setting.isGroup())
    throw Fmi::Exception(BCP, "The layers of a basemap must be groups")
        .addParameter("Setting", setting.getPath());
  if (!setting.lookupValue("source", layer.source) || layer.source.empty())
    throw Fmi::Exception(BCP, "A basemap layer needs a source")
        .addParameter("Setting", setting.getPath());
  setting.lookupValue("layer", layer.layer);
  setting.lookupValue("where", layer.where);
  return layer;
}

// Appends ring or line as [lon0, lat0, dlon1, dlat1, ...] in 1/scale degrees, the repeated
// points of the precision left out, unless it collapses to fewer than min_points points
void encode(const OGRSimpleCurve& curve, int scale, std::size_t min_points, std::string& out,
            bool& first)
{
  std::vector<long> values;
  values.reserve(2 * curve.getNumPoints());
  bool have = false;
  long px = 0;
  long py = 0;
  for (int i = 0; i < curve.getNumPoints(); i++)
  {
    const long x = std::lround(curve.getX(i) * scale);
    const long y = std::lround(curve.getY(i) * scale);
    if (have && x == px && y == py)
      continue;
    values.push_back(have ? x - px : x);
    values.push_back(have ? y - py : y);
    px = x;
    py = y;
    have = true;
  }
  if (values.size() < 2 * min_points)
    return;
  out += first ? "[" : ",[";
  first = false;
  for (std::size_t i = 0; i < values.size(); i++)
    out += (i == 0 ? "" : ",") + std::to_string(values[i]);
  out += ']';
}

// The rings of the polygons or the lines of a geometry
void encodeGeometry(const OGRGeometry& geom, bool polygons, int scale, std::string& out,
                    bool& first)
{
  const auto type = wkbFlatten(geom.getGeometryType());
  if (polygons && type == wkbPolygon)
  {
    for (const auto* ring : *geom.toPolygon())
      encode(*ring, scale, 4, out, first);
  }
  else if (!polygons && type == wkbLineString)
    encode(*geom.toLineString(), scale, 2, out, first);
  else if (type == wkbMultiPolygon || type == wkbMultiLineString ||
           type == wkbGeometryCollection)
  {
    for (const auto* part : *geom.toGeometryCollection())
      encodeGeometry(*part, polygons, scale, out, first);
  }
}

// The features of a layer as the JSON array of its rings or lines
std::string encodeLayer(const BasemapLayerSource& source, bool polygons, int scale)
{
  std::string out = "[";
  if (source.source.empty())
    return out + "]";

  // The drivers may not be registered yet when the catalog is created
  GDALAllRegister();
  std::unique_ptr<GDALDataset> ds(static_cast<GDALDataset*>(GDALOpenEx(
      source.source.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr)));
  if (!ds)
    throw Fmi::Exception(BCP, "Cannot open the data source of a basemap layer")
        .addParameter("Source", redactedSource(source.source));

  OGRLayer* layer = source.layer.empty() ? ds->GetLayer(0) : ds->GetLayerByName(source.layer.c_str());
  if (layer == nullptr)
    throw Fmi::Exception(BCP, "The basemap layer was not found")
        .addParameter("Source", redactedSource(source.source))
        .addParameter("Layer", source.layer);
  if (!source.where.empty() && layer->SetAttributeFilter(source.where.c_str()) != OGRERR_NONE)
    throw Fmi::Exception(BCP, "Invalid attribute filter of a basemap layer")
        .addParameter("Layer", source.layer)
        .addParameter("Where", source.where);

  // The coordinates in longitude and latitude
  std::unique_ptr<OGRCoordinateTransformation> transformation;
  const OGRSpatialReference* srs = layer->GetSpatialRef();
  OGRSpatialReference wgs84;
  wgs84.SetWellKnownGeogCS("WGS84");
  wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  if (srs != nullptr && !srs->IsSame(&wgs84))
  {
    OGRSpatialReference from(*srs);
    from.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    transformation.reset(OGRCreateCoordinateTransformation(&from, &wgs84));
    if (!transformation)
      throw Fmi::Exception(BCP, "Cannot convert a basemap layer to longitude and latitude")
          .addParameter("Layer", source.layer);
  }

  bool first = true;
  layer->ResetReading();
  for (auto& feature : *layer)
  {
    const OGRGeometry* geom = feature->GetGeometryRef();
    if (geom == nullptr)
      continue;
    if (transformation)
    {
      std::unique_ptr<OGRGeometry> copy(geom->clone());
      if (copy->transform(transformation.get()) != OGRERR_NONE)
        continue;
      encodeGeometry(*copy, polygons, scale, out, first);
    }
    else
      encodeGeometry(*geom, polygons, scale, out, first);
  }
  return out + "]";
}
}  // namespace

BasemapSources BasemapSources::parse(const libconfig::Setting& parent, const std::string& name)
{
  try
  {
    BasemapSources sources;
    if (!parent.exists(name))
      return sources;
    const auto& group = parent[name.c_str()];
    if (!group.isGroup())
      throw Fmi::Exception(BCP, "A basemap must be a group").addParameter("Setting", group.getPath());
    sources.land = parseLayer(group, "land");
    sources.lakes = parseLayer(group, "lakes");
    sources.borders = parseLayer(group, "borders");
    if (!sources.configured())
      throw Fmi::Exception(BCP, "A basemap needs its land layer")
          .addParameter("Setting", group.getPath());
    return sources;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse the basemap settings");
  }
}

std::string basemapJson(const BasemapSources& sources, int scale, const std::string& title)
{
  try
  {
    std::string json = fmt::format(R"({{"source":"{}","scale":{})", title, scale);
    json += R"(,"land":)" + encodeLayer(sources.land, true, scale);
    json += R"(,"lakes":)" + encodeLayer(sources.lakes, true, scale);
    json += R"(,"borders":)" + encodeLayer(sources.borders, false, scale);
    return json + "}";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to read the basemap");
  }
}

std::string redactedSource(const std::string& source)
{
  // password=secret or password='a secret' in a PostgreSQL connection string
  std::string out = source;
  auto pos = out.find("password=");
  while (pos != std::string::npos)
  {
    const auto start = pos + 9;
    auto end = start;
    if (end < out.size() && out[end] == '\'')
    {
      end = out.find('\'', end + 1);
      end = (end == std::string::npos ? out.size() : end + 1);
    }
    else
      end = std::min(out.find(' ', start), out.size());
    out.replace(start, end - start, "***");
    pos = out.find("password=", start + 3);
  }
  return out;
}

}  // namespace OpenAPI
}  // namespace Spine
}  // namespace SmartMet
