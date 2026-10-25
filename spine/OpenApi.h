// ======================================================================
/*!
 * \brief OpenAPI definitions for SmartMet Server APIs
 *
 * Shared building blocks for the OpenAPI 3.1 definitions that the plugins
 * serve for their APIs (OGC API - EDR, Tiles, Coverages, WMTS, TimeSeries).
 *
 * Every definition is generated from code, configuration and metadata. The
 * owning plugin builds a Document whenever its metadata changes and hands it
 * to a Publisher, which serializes it once. Requests only insert the server
 * URL of the API into the serialized text.
 *
 * Each API answers three kinds of resources below its root path:
 *
 *   <root>/api              the OpenAPI definition (JSON), or the
 *                           documentation page for f=html and for clients
 *                           preferring text/html
 *   <root>/api/docs         human-readable documentation (Swagger UI)
 *   <root>/api/docs/<file>  Swagger UI files, served from a local directory
 *                           (package smartmet-swagger-ui), never from a CDN,
 *                           and the SmartMet plugin smartmet-swagger-ui.js
 *
 * Configuration (the same group in every plugin configuration file):
 *
 *   openapi:
 *   {
 *     enabled            = true;
 *     ui                 = true;
 *     swagger_ui_dir     = "/usr/share/smartmet/swagger-ui";
 *     expand_collections = true;
 *     max_enum           = 1000;
 *     map_tiles          = "/tiles/collections/<layer>/tiles/EPSG:3857/{z}/{y}/{x}?f=png";
 *     map_tile_size      = 256;
 *     map_attribution    = "...";
 *     contact            = { name = "..."; url = "..."; email = "..."; };
 *     license            = { name = "..."; url = "..."; };
 *   };
 */
// ======================================================================

#pragma once

#include "Basemap.h"

#include <json/json.h>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace libconfig
{
class Config;
}  // namespace libconfig

namespace SmartMet
{
namespace Spine
{
namespace IPFilter
{
class IPFilter;
}  // namespace IPFilter

namespace HTTP
{
class Request;
class Response;
}  // namespace HTTP

namespace OpenAPI
{
// The OpenAPI version of all generated definitions
extern const char* const OPENAPI_VERSION;  // "3.1.1"

// Media type of the JSON encoded definition
extern const char* const MEDIA_TYPE;  // "application/vnd.oai.openapi+json;version=3.1"

// OGC link relation types used in landing pages
extern const char* const REL_SERVICE_DESC;  // OpenAPI definition
extern const char* const REL_SERVICE_DOC;   // human-readable documentation
extern const char* const REL_CONFORMANCE;
extern const char* const REL_DATA;

// ----------------------------------------------------------------------
/*!
 * \brief The "openapi" configuration group
 */
// ----------------------------------------------------------------------

struct Settings
{
  bool enabled = true;
  bool ui = true;                  // serve <root>/api/docs
  bool expand_collections = true;  // per-collection paths with exact enums
  std::size_t max_enum = 1000;     // larger value sets are not listed
  std::string swagger_ui_dir = "/usr/share/smartmet/swagger-ui";
  // Background map of the location maps of the documentation pages: a Web Mercator
  // (EPSG:3857) tile URL template with {z}, {x} and {y}, normally an OGC API - Tiles layer
  // of the same server. Empty for no background.
  std::string map_tiles;
  int map_tile_size = 256;
  std::string map_attribution;
  Json::Value contact;  // OpenAPI contact object, null if not set
  Json::Value license;  // OpenAPI license object, null if not set

  // Read the group at 'path'. A missing group yields the defaults.
  static Settings parse(const libconfig::Config& config, const std::string& path = "openapi");
};

// ----------------------------------------------------------------------
/*!
 * \brief JSON helpers for building definitions
 */
// ----------------------------------------------------------------------

// {"$ref": "#/components/<kind>/<name>"}
Json::Value ref(const std::string& kind, const std::string& name);

// {"type": "string"} with an optional description
Json::Value stringSchema(const std::string& description = "");

// {"type": "string", "enum": [...]}. When there are more values than
// maxEnum, the enum is left out and the description says where to find the
// values; valuesUrl, if given, is added as the "x-values-url" extension.
Json::Value enumSchema(const std::vector<std::string>& values,
                       std::size_t maxEnum,
                       const std::string& valuesUrl = "");

// {"type": "array", "items": items}
Json::Value arraySchema(const Json::Value& items);

// A parameter object. Array-valued query parameters are comma-separated
// lists (style "form", explode false), as SmartMet parses them.
Json::Value parameter(const std::string& name,
                      const std::string& in,
                      const std::string& description,
                      const Json::Value& schema,
                      bool required = false);

// A link object for landing pages and other link lists
Json::Value link(const std::string& href,
                 const std::string& rel,
                 const std::string& type,
                 const std::string& title);

// Names of the Swagger UI files served below <root>/api/docs/
std::vector<std::string> documentationFiles();

// The service-desc and service-doc links of a landing page. 'root' is the
// API root URL as used in the page's other links.
Json::Value serviceDescLink(const std::string& root);
Json::Value serviceDocLink(const std::string& root);

// ----------------------------------------------------------------------
/*!
 * \brief A mutable OpenAPI definition under construction
 */
// ----------------------------------------------------------------------

class Document
{
 public:
  Document(const std::string& title,
           const std::string& description,
           const std::string& version,
           const Settings& settings);

  // The whole definition
  const Json::Value& json() const { return itsDoc; }
  Json::Value& json() { return itsDoc; }

  void addTag(const std::string& name, const std::string& description);

  // The operation object for 'method' ("get", "post") at 'path', created
  // empty if necessary. 'path' is relative to the server URL.
  Json::Value& operation(const std::string& path, const std::string& method);

  // Register a reusable component, e.g. kind "parameters", "schemas",
  // "responses". An existing component of the same name is replaced.
  void addComponent(const std::string& kind, const std::string& name, const Json::Value& value);
  bool hasComponent(const std::string& kind, const std::string& name) const;

  // Register the OGC API - Common components:
  //   parameters: bbox, bbox-crs, datetime, limit, crs
  //   schemas:    link, landingPage, confClasses, exception
  //   responses:  LandingPage, ConformanceDeclaration, BadRequest, NotFound,
  //               NotAcceptable, ServerError
  void addCommonComponents();

  // Add the operations every OGC API has: GET /, /conformance, /api,
  // /api/docs and /api/docs/{file}. 'formatParameter' is added to the landing page and
  // conformance operations unless it is null.
  void addCoreOperations(const std::string& tag, const Json::Value& formatParameter);

  // Add only GET <prefix>/api, <prefix>/api/docs and <prefix>/api/docs/{file}, for APIs
  // without an OGC API landing page. 'prefix' is the path of the API root relative to the
  // server URL, empty if the server URL is the API root.
  void addApiOperations(const std::string& tag, const std::string& prefix = "");

  // The standard error responses (400, 404, 500) as references
  static void addErrorResponses(Json::Value& operation, bool notFound = true);

 private:
  Json::Value itsDoc;
};

// ----------------------------------------------------------------------
/*!
 * \brief An immutable, serialized definition
 */
// ----------------------------------------------------------------------

class Definition
{
 public:
  explicit Definition(const Document& doc);

  // The JSON text with 'serverUrl' as the URL of the only server
  std::string json(const std::string& serverUrl) const;

  // Hash of the serialized definition, excluding the server URL
  std::size_t hash() const { return itsHash; }

  const std::string& title() const { return itsTitle; }

 private:
  std::string itsPrefix;  // text before the server URL
  std::string itsSuffix;  // text after the server URL
  std::string itsTitle;
  std::size_t itsHash = 0;
};

// ----------------------------------------------------------------------
/*!
 * \brief Serves the definition and the documentation of one API
 *
 * Thread safe: publish() may be called while requests are being served.
 */
// ----------------------------------------------------------------------

class Publisher
{
 public:
  explicit Publisher(Settings settings);
  ~Publisher();
  Publisher(const Publisher&) = delete;
  Publisher& operator=(const Publisher&) = delete;
  Publisher(Publisher&&) = delete;
  Publisher& operator=(Publisher&&) = delete;

  const Settings& settings() const { return itsSettings; }

  // List the API at 'rootPath' in the API catalog of the server (see Catalog) for as long
  // as this publisher exists. An empty title is replaced by the title of the published
  // definition. Does nothing if the definitions are disabled.
  void addToCatalog(const std::string& rootPath,
                    const std::string& title = "",
                    const std::string& description = "");

  // The same with the kind of the API, which tells clients such as the data explorer what
  // the API is: "edr", "timeseries", "coverages", "tiles" or "wmts".
  void addToCatalog(const std::string& rootPath,
                    const std::string& title,
                    const std::string& description,
                    const std::string& kind);

  // Replace the served definition
  void publish(const Document& doc);
  std::shared_ptr<const Definition> definition() const;

  // True if 'resource' is <rootPath>/api, <rootPath>/api/docs or a file
  // below it. 'rootPath' is the path the API is registered at, e.g. "/edr".
  static bool isApiResource(const std::string& resource, const std::string& rootPath);

  // Answer a request for one of the resources above. 'serverUrl' is the URL
  // clients use for the API root, see serverUrl(). Returns false if the
  // resource is not an API resource or the definition is disabled.
  bool handle(const HTTP::Request& request,
              HTTP::Response& response,
              const std::string& rootPath,
              const std::string& serverUrl) const;

  // The same with a definition of the caller's own, for example one generated for the
  // access rights of the client, instead of the published one
  bool handle(const HTTP::Request& request,
              HTTP::Response& response,
              const std::string& rootPath,
              const std::string& serverUrl,
              const std::shared_ptr<const Definition>& definition) const;

 private:
  friend class Catalog;

  void handleDefinition(const HTTP::Request& request,
                        HTTP::Response& response,
                        const std::string& serverUrl,
                        const std::shared_ptr<const Definition>& definition) const;
  void handleDocs(const HTTP::Request& request,
                  HTTP::Response& response,
                  const std::string& definitionHref,
                  const std::string& assetPrefix,
                  const std::shared_ptr<const Definition>& definition) const;
  void handleAsset(const HTTP::Request& request,
                   HTTP::Response& response,
                   const std::string& name) const;

  struct Asset
  {
    std::string content;
    std::string contentType;
    std::string etag;
  };

  Settings itsSettings;
  bool itsSwaggerUi = false;                        // all Swagger UI files were found
  std::map<std::string, Asset> itsAssets;           // documentation files by name
  std::shared_ptr<const Definition> itsDefinition;  // accessed atomically
};

// ----------------------------------------------------------------------
/*!
 * \brief The API catalog of a server (RFC 9727)
 *
 * Lists the APIs of all publishers added with Publisher::addToCatalog(), or
 * on a frontend the merged catalogs of the backends. Served at
 *
 *   /.well-known/api-catalog  the catalog as a linkset (application/linkset+json)
 *   /api                      the same, or an HTML page for f=html and for clients
 *                             preferring text/html
 *   /api/docs                 Swagger UI with all the APIs in a selector
 *   /api/docs/<file>          Swagger UI files
 *   /api/explorer             the data explorer: queries built from the metadata of
 *                             the APIs, with maps and charts. Private: it is not in the
 *                             URI list of the server, and only the clients allowed by
 *                             explorer_ip_filters (local and private networks by default)
 *                             may use it or see the link to it on the /api page
 *   /api/explorer/<file>      the files of the explorer
 *
 * Configuration (the reactor configuration file):
 *
 *   api_catalog:
 *   {
 *     enabled        = true;
 *     ui             = true;
 *     swagger_ui_dir = "/usr/share/smartmet/swagger-ui";
 *     title          = "SmartMet Server APIs";
 *     description    = "...";
 *     explorer       = true;
 *     explorer_dir   = "/usr/share/smartmet/explorer";
 *     explorer_ip_filters = ["10.0.0.0/8", ...];  // default: local and private networks
 *     uplot_dir      = "/usr/share/smartmet/uplot";
 *     palette_dir    = "/usr/share/smartmet/qdless/palettes";
 *     palette_config = "/usr/share/smartmet/qdless/qdless.conf";
 *
 *     // The defaults of the explorer for the region and the users of the server, all optional
 *     explorer_defaults:
 *     {
 *       center      = [25.0, 62.0];      // the first view of the maps: longitude, latitude
 *       zoom        = 4;                 // and zoom level (0-18)
 *       // The examples of the location fields; points, boxes and paths are made from the center
 *       examples:
 *       {
 *         place   = "Helsinki";
 *         fmisid  = "100971,101004";
 *         wmo     = "2963,2974";
 *         keyword = "synop_fi";
 *         area    = "Uusimaa";
 *       };
 *       // Output projections offered by the Coverages module besides the native grid,
 *       // EPSG:4326 and EPSG:3857: "code label"
 *       projections = ["EPSG:3067 ETRS-TM35FIN", "EPSG:3035 ETRS89 LAEA Europe"];
 *       language    = "fi";              // en, or one of the translations explorer-<code>.js
 *       clock       = "24";              // "24" or "12" hours, by default that of the browser
 *       // The units shown until the user chooses: temperature C|F|K, speed ms|kt|kmh|mph,
 *       // pressure hPa|Pa|inHg, height m|ft. By default C, m/s, hPa and m, or F, mph and ft
 *       // for browsers whose language region uses them (e.g. en-US).
 *       units:
 *       {
 *         temperature = "C";
 *         speed       = "kt";
 *       };
 *     };
 *
 *     // The background maps of the explorer: land, lakes and country borders. By default the
 *     // files basemap.json (Natural Earth 1:50m) and basemap-10m.json (1:10m, drawn on
 *     // enlarged and zoomed in maps) of explorer_dir. Any OGR data source can be read instead:
 *     // a shapefile, a GeoJSON file or a PostGIS database ("PG:..."), with a layer (the first
 *     // one by default) and an attribute filter (all features by default). Land is required,
 *     // lakes and borders are optional. A source that cannot be read leaves the file in use.
 *     basemap_detail:
 *     {
 *       land    = { source = "/smartmet/share/naturalearth/ne_10m_land.shp"; };
 *       lakes   = { source = "PG:host=gis dbname=gis user=reader"; layer = "natural_earth.ne_10m_lakes"; };
 *       borders = { source = "PG:host=gis dbname=gis user=reader";
 *                   layer  = "natural_earth.ne_10m_admin_0_boundary_lines_land"; };
 *     };
 *     // basemap: the same for the 1:50m map
 *   };
 */
// ----------------------------------------------------------------------

// The media type of the catalog
extern const char* const CATALOG_MEDIA_TYPE;  // application/linkset+json with the RFC 9727 profile

struct CatalogEntry
{
  std::string root;  // path of the API root, e.g. "/edr"
  std::string title;
  std::string description;
  std::string kind;  // see Publisher::addToCatalog()
  bool docs = true;  // <root>/api/docs is available

  bool operator==(const CatalogEntry& other) const
  {
    return root == other.root && title == other.title && description == other.description &&
           kind == other.kind && docs == other.docs;
  }
};

struct CatalogSettings
{
  Settings openapi;  // enabled, ui and swagger_ui_dir apply
  std::string title = "SmartMet Server APIs";
  std::string description;
  bool explorer = true;  // serve /api/explorer
  std::string explorer_dir = "/usr/share/smartmet/explorer";
  // The clients that may use the explorer: explorer_ip_filters, or the local and private
  // networks. Its requests can be arbitrarily large, so it is for the staff of the site only.
  std::shared_ptr<IPFilter::IPFilter> explorer_ip_filter;
  std::string uplot_dir = "/usr/share/smartmet/uplot";
  // The isoband colours of the production products (as converted by qdless from wms-conf)
  // and the parameter to palette map, used for the values on the maps of the explorer
  std::string palette_dir = "/usr/share/smartmet/qdless/palettes";
  std::string palette_config = "/usr/share/smartmet/qdless/qdless.conf";
  // The validated explorer_defaults group as JSON for the explorer page, null when not set
  Json::Value explorer_defaults;
  // The background maps of the explorer from OGR data sources instead of the files
  // basemap.json and basemap-10m.json of explorer_dir, when configured
  BasemapSources basemap;
  BasemapSources basemap_detail;

  // Read the group at 'path'. A missing group yields the defaults.
  static CatalogSettings parse(const libconfig::Config& config,
                               const std::string& path = "api_catalog");
};

// The APIs added to the catalog in this process, sorted by root
std::vector<CatalogEntry> localCatalogEntries();

// The entries of a catalog linkset as generated by Catalog. Entries whose root is not a
// plain path are skipped. Absolute URLs are reduced to their paths, without an apikey prefix.
std::vector<CatalogEntry> parseCatalog(const Json::Value& linkset);

class Catalog
{
 public:
  explicit Catalog(CatalogSettings settings);
  ~Catalog();
  Catalog(const Catalog&) = delete;
  Catalog& operator=(const Catalog&) = delete;
  Catalog(Catalog&&) = delete;
  Catalog& operator=(Catalog&&) = delete;

  const CatalogSettings& settings() const { return itsSettings; }

  // True for the resources listed above
  static bool isCatalogResource(const std::string& resource);

  // The catalog as a linkset. URLs are absolute when the request has a valid Host.
  static Json::Value linkset(const HTTP::Request& request,
                             const std::vector<CatalogEntry>& entries);

  // Answer a request for one of the resources above with 'entries'. Returns false if the
  // resource is not a catalog resource or the catalog is disabled.
  bool handle(const HTTP::Request& request,
              HTTP::Response& response,
              const std::vector<CatalogEntry>& entries) const;

 private:
  void handleLinkset(const HTTP::Request& request,
                     HTTP::Response& response,
                     const std::vector<CatalogEntry>& entries) const;
  void handlePage(const HTTP::Request& request,
                  HTTP::Response& response,
                  const std::vector<CatalogEntry>& entries) const;
  void handleDocs(const HTTP::Request& request,
                  HTTP::Response& response,
                  const std::vector<CatalogEntry>& entries) const;
  void handleExplorer(const HTTP::Request& request, HTTP::Response& response) const;
  bool explorerAllowed(const HTTP::Request& request) const;
  void handleExplorerFile(const HTTP::Request& request,
                          HTTP::Response& response,
                          const std::string& name) const;

  CatalogSettings itsSettings;
  std::unique_ptr<Publisher> itsFiles;  // the Swagger UI files

  // The files of the explorer by name, empty if some file is missing
  struct ExplorerFile
  {
    std::string content;
    std::string contentType;
    std::string etag;
  };
  std::map<std::string, ExplorerFile> itsExplorerFiles;
};

// ----------------------------------------------------------------------
/*!
 * \brief The URL clients use for an API root
 *
 * Scheme (X-Forwarded-Proto: http or https only), Host and the apikey path
 * prefix of the request, followed by 'rootPath'. If the Host header is
 * missing or is not a plain hostname[:port], the relative URL 'rootPath' is
 * returned instead, which OpenAPI resolves against the definition's own URL.
 */
// ----------------------------------------------------------------------

std::string serverUrl(const HTTP::Request& request, const std::string& rootPath);

// ----------------------------------------------------------------------
/*!
 * \brief Whether the client prefers HTML over JSON
 *
 * True if 'f' is "html", or if there is no 'f' and the Accept header gives
 * text/html a higher quality than any JSON media type or wildcard.
 */
// ----------------------------------------------------------------------

bool prefersHtml(const HTTP::Request& request);

}  // namespace OpenAPI
}  // namespace Spine
}  // namespace SmartMet
