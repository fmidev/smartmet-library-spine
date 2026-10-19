// ======================================================================
/*!
 * \brief The background maps of the data explorer from OGR data sources
 *
 * The explorer draws its location maps over land, lakes and country borders. By default they
 * come from the files basemap.json (Natural Earth 1:50m) and basemap-10m.json (1:10m, for
 * enlarged and zoomed in maps) of the explorer directory, made by explorer/make-basemap.py.
 * A server with the data on disk or in a database can read the layers from there instead: any
 * OGR data source (a shapefile, a GeoJSON file, or a PostGIS database as a "PG:..."
 * connection string) and layer.
 */
// ======================================================================

#pragma once

#include <string>

namespace libconfig
{
class Setting;
}  // namespace libconfig

namespace SmartMet
{
namespace Spine
{
namespace OpenAPI
{
// A layer of a background map: an OGR data source, the layer (the first one by default) and
// an attribute filter of its features (all by default)
struct BasemapLayerSource
{
  std::string source;
  std::string layer;
  std::string where;
};

// The land, lakes and borders of a background map. Land is required when the map is
// configured, the others are optional.
struct BasemapSources
{
  BasemapLayerSource land;
  BasemapLayerSource lakes;
  BasemapLayerSource borders;

  bool configured() const { return !land.source.empty(); }

  // Read the group at 'path', e.g. api_catalog.basemap. A missing group is not configured.
  static BasemapSources parse(const libconfig::Setting& parent, const std::string& name);
};

// The map in the format of basemap.json (see explorer/make-basemap.py): the coordinates in
// longitude and latitude rounded to 1/scale degrees and delta encoded. Throws if a layer
// cannot be read.
std::string basemapJson(const BasemapSources& sources, int scale, const std::string& title);

// The data source with a password of a connection string hidden, for messages
std::string redactedSource(const std::string& source);

}  // namespace OpenAPI
}  // namespace Spine
}  // namespace SmartMet
