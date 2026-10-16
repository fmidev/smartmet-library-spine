#include "OpenApi.h"
#include "Convenience.h"
#include "FmiApiKey.h"
#include "HTTP.h"
#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <libconfig.h++>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

namespace SmartMet
{
namespace Spine
{
namespace OpenAPI
{
const char* const OPENAPI_VERSION = "3.1.1";
const char* const MEDIA_TYPE = "application/vnd.oai.openapi+json;version=3.1";
const char* const REL_SERVICE_DESC = "service-desc";
const char* const REL_SERVICE_DOC = "service-doc";
const char* const REL_CONFORMANCE = "http://www.opengis.net/def/rel/ogc/1.0/conformance";
const char* const REL_DATA = "http://www.opengis.net/def/rel/ogc/1.0/data";
const char* const CATALOG_MEDIA_TYPE =
    "application/linkset+json; profile=\"https://www.rfc-editor.org/info/rfc9727\"";

// The SmartMet Swagger UI plugin, see OpenApiPlugin.cpp
extern const char* const SWAGGER_UI_PLUGIN_JS;

namespace
{
// Placeholder for the server URL in the serialized definition
const char* const SERVER_URL_PLACEHOLDER = "__SMARTMET_OPENAPI_SERVER_URL__";

const char* const PLUGIN_FILE = "smartmet-swagger-ui.js";

// Swagger UI files served below <root>/api/docs/
const std::vector<std::pair<std::string, std::string>> swagger_ui_files{
    {"swagger-ui-bundle.js", "text/javascript; charset=UTF-8"},
    {"swagger-ui-standalone-preset.js", "text/javascript; charset=UTF-8"},
    {"swagger-ui.css", "text/css; charset=UTF-8"},
    {"favicon-16x16.png", "image/png"},
    {"favicon-32x32.png", "image/png"}};

// Cache lifetimes. The definition follows metadata changes, the Swagger UI
// files change only when the package is upgraded.
const char* const DEFINITION_CACHE_CONTROL = "public, max-age=60";
const char* const ASSET_CACHE_CONTROL = "public, max-age=86400";

// The documentation page loads only its own files and the definition. 'unsafe-inline' is
// needed for the initializer script and the styles Swagger UI sets on elements.
const char* const DOCS_CONTENT_SECURITY_POLICY =
    "default-src 'none'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data:; font-src 'self' data:; connect-src 'self'; base-uri 'none'; "
    "form-action 'self'; frame-ancestors 'self'";

// The origin of an absolute tile URL for the Content-Security-Policy, or an empty string if
// the URL is relative or the origin contains anything but host name and port characters
std::string tileOrigin(const std::string& url)
{
  std::size_t start = 0;
  if (url.compare(0, 8, "https://") == 0)
    start = 8;
  else if (url.compare(0, 7, "http://") == 0)
    start = 7;
  else
    return {};
  const auto end = url.find('/', start);
  if (end == std::string::npos || end == start)
    return {};
  for (std::size_t i = start; i < end; i++)
  {
    const char ch = url[i];
    if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '.' && ch != '-' && ch != ':')
      return {};
  }
  return url.substr(0, end);
}

std::string toJsonText(const Json::Value& value)
{
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  builder["emitUTF8"] = true;
  return Json::writeString(builder, value);
}

// The JSON string literal for 'text' without the surrounding quotes
std::string jsonEscape(const std::string& text)
{
  auto quoted = toJsonText(Json::Value(text));
  return quoted.substr(1, quoted.size() - 2);
}

std::string htmlEscape(const std::string& text)
{
  std::string ret;
  ret.reserve(text.size());
  for (char c : text)
  {
    switch (c)
    {
      case '&':
        ret += "&amp;";
        break;
      case '<':
        ret += "&lt;";
        break;
      case '>':
        ret += "&gt;";
        break;
      case '"':
        ret += "&quot;";
        break;
      case '\'':
        ret += "&#39;";
        break;
      default:
        ret += c;
    }
  }
  return ret;
}

// The definition embeds the apikey of the client in the server URL, and may list only what
// the apikey has access to, so it must not be stored in shared caches
std::string cacheControl(const HTTP::Request& request)
{
  if (FmiApiKey::getFmiApiKey(request, true))
    return "private, max-age=60";
  return DEFINITION_CACHE_CONTROL;
}

std::string quotedETag(std::size_t hash, const char* kind)
{
  return fmt::format("\"{:x}-openapi-{}\"", hash, kind);
}

// Handle X-Request-ETag probes and conditional requests. Returns true if the
// response is complete.
bool etagOnly(const HTTP::Request& request, HTTP::Response& response, const std::string& etag)
{
  response.setHeader("ETag", etag);
  if (request.getHeader("X-Request-ETag"))
  {
    response.setStatus(HTTP::Status::no_content);
    return true;
  }
  if (auto status = HTTP::conditionalResponseStatus(request, etag))
  {
    response.setStatus(*status);
    return true;
  }
  return false;
}

void problem(HTTP::Response& response,
             int status,
             const std::string& title,
             const std::string& detail)
{
  Json::Value doc;
  doc["title"] = title;
  doc["status"] = status;
  doc["detail"] = detail;
  response.setStatus(status);
  response.setHeader("Content-Type", "application/problem+json");
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setContent(toJsonText(doc));
}

std::vector<std::string> pathSegments(const std::string& path)
{
  std::vector<std::string> parts;
  boost::algorithm::split(parts, path, boost::algorithm::is_any_of("/"));
  std::vector<std::string> ret;
  for (auto& part : parts)
    if (!part.empty())
      ret.push_back(std::move(part));
  return ret;
}

// Relative reference from the document at 'from' to the absolute path 'to'.
// Both are server-side paths, so the reference also works behind proxies
// that add a prefix such as an apikey path.
std::string relativeHref(const std::string& from, const std::string& to)
{
  // Directory of the referencing document
  auto from_dir = pathSegments(from.substr(0, from.rfind('/') + 1));
  auto target = pathSegments(to);
  const bool target_is_dir = (!to.empty() && to.back() == '/');

  std::size_t common = 0;
  while (common < from_dir.size() && common < target.size() && from_dir[common] == target[common])
    ++common;

  // A file target must be named by its last segment, so step out of a
  // directory of the same name (from /a/b/c/ to /a/b is "../../b", not "../")
  if (!target_is_dir && common > 0 && common == target.size())
    --common;

  std::string ret;
  for (std::size_t i = common; i < from_dir.size(); i++)
    ret += "../";
  for (std::size_t i = common; i < target.size(); i++)
  {
    ret += target[i];
    if (i + 1 < target.size() || target_is_dir)
      ret += '/';
  }
  if (ret.empty())
    ret = "./";
  return ret;
}

// Quality value of a media range parameter list, 1 if absent
double qualityValue(const std::vector<std::string>& parts)
{
  for (std::size_t i = 1; i < parts.size(); i++)
  {
    auto param = Fmi::trim_copy(parts[i]);
    if (boost::algorithm::istarts_with(param, "q="))
    {
      try
      {
        return std::stod(param.substr(2));
      }
      catch (...)
      {
        return 0;
      }
    }
  }
  return 1;
}

bool validHost(const std::string& host)
{
  if (host.empty() || host.size() > 253)
    return false;
  for (char c : host)
  {
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == '.' || c == '-' || c == ':' || c == '[' || c == ']';
    if (!ok)
      return false;
  }
  return true;
}

bool validApiKey(const std::string& key)
{
  if (key.empty() || key.size() > 128)
    return false;
  for (char c : key)
  {
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.';
    if (!ok)
      return false;
  }
  return true;
}

Json::Value objectSetting(const libconfig::Setting& group,
                          const std::string& name,
                          const std::vector<std::string>& fields)
{
  Json::Value ret;
  if (!group.exists(name))
    return ret;
  const auto& setting = group[name.c_str()];
  if (!setting.isGroup())
    throw Fmi::Exception(BCP, "OpenAPI setting must be a group").addParameter("Setting", name);
  for (const auto& field : fields)
  {
    std::string value;
    if (setting.lookupValue(field, value))
      ret[field] = value;
  }
  return ret;
}

// The content security policy of the documentation pages. The origin of an absolute tile
// URL is allowed for the location maps.
std::string docsPolicy(const Settings& settings)
{
  std::string policy = DOCS_CONTENT_SECURITY_POLICY;
  const auto origin = tileOrigin(settings.map_tiles);
  if (!origin.empty())
    boost::algorithm::replace_first(policy, "img-src 'self'", "img-src 'self' " + origin);
  return policy;
}

// ----------------------------------------------------------------------
// The API catalog registry
// ----------------------------------------------------------------------

struct CatalogRegistration
{
  std::uint64_t order = 0;  // later registrations win for the same root
  CatalogEntry entry;
};

struct CatalogRegistry
{
  std::mutex mutex;
  std::uint64_t counter = 0;
  std::map<const Publisher*, CatalogRegistration> entries;
};

// Never destroyed, since publishers may be destroyed during static destruction
CatalogRegistry& catalogRegistry()
{
  static auto* registry = new CatalogRegistry;  // NOLINT(cppcoreguidelines-owning-memory)
  return *registry;
}

// A plain absolute path usable as an API root
bool validRootPath(const std::string& path)
{
  if (path.size() < 2 || path.size() > 256 || path[0] != '/' || path.back() == '/')
    return false;
  if (path.find("//") != std::string::npos || path.find("/./") != std::string::npos ||
      path.find("/../") != std::string::npos || boost::algorithm::ends_with(path, "/..") ||
      boost::algorithm::ends_with(path, "/."))
    return false;
  for (char c : path)
  {
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == '/' || c == '-' || c == '_' || c == '.' || c == '~';
    if (!ok)
      return false;
  }
  return true;
}

// The path of a catalog URL: the origin of an absolute URL and an apikey prefix are removed.
// Returns an empty string for anything else than a plain path.
std::string catalogPath(const std::string& href)
{
  std::string path = href;
  for (const char* scheme : {"https://", "http://"})
  {
    if (boost::algorithm::starts_with(path, scheme))
    {
      const auto pos = path.find('/', std::strlen(scheme));
      if (pos == std::string::npos)
        return {};
      path = path.substr(pos);
      break;
    }
  }
  if (boost::algorithm::starts_with(path, "/fmi-apikey/"))
  {
    const auto pos = path.find('/', 12);
    if (pos == std::string::npos)
      return {};
    path = path.substr(pos);
  }
  return validRootPath(path) ? path : std::string();
}

}  // namespace

// ----------------------------------------------------------------------
// Settings
// ----------------------------------------------------------------------

Settings Settings::parse(const libconfig::Config& config, const std::string& path)
{
  try
  {
    Settings settings;
    if (!config.exists(path))
      return settings;

    const auto& group = config.lookup(path);
    if (!group.isGroup())
      throw Fmi::Exception(BCP, "OpenAPI settings must be a group").addParameter("Setting", path);

    group.lookupValue("enabled", settings.enabled);
    group.lookupValue("ui", settings.ui);
    group.lookupValue("expand_collections", settings.expand_collections);
    group.lookupValue("swagger_ui_dir", settings.swagger_ui_dir);
    group.lookupValue("map_tiles", settings.map_tiles);
    group.lookupValue("map_tile_size", settings.map_tile_size);
    group.lookupValue("map_attribution", settings.map_attribution);
    if (settings.map_tile_size < 64 || settings.map_tile_size > 4096)
      throw Fmi::Exception(BCP, "openapi.map_tile_size must be in the range 64...4096");
    if (!settings.map_tiles.empty() && settings.map_tiles[0] != '/' &&
        tileOrigin(settings.map_tiles).empty())
      throw Fmi::Exception(BCP,
                           "openapi.map_tiles must be a path or an http(s) URL with a plain "
                           "host name")
          .addParameter("map_tiles", settings.map_tiles);

    int max_enum = 0;
    if (group.lookupValue("max_enum", max_enum))
    {
      if (max_enum < 0)
        throw Fmi::Exception(BCP, "openapi.max_enum must be non-negative");
      settings.max_enum = static_cast<std::size_t>(max_enum);
    }

    settings.contact = objectSetting(group, "contact", {"name", "url", "email"});
    settings.license = objectSetting(group, "license", {"name", "url"});

    return settings;
  }
  catch (const libconfig::SettingException& e)
  {
    throw Fmi::Exception(BCP, "Invalid OpenAPI configuration").addParameter("Path", e.getPath());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse OpenAPI settings");
  }
}

// ----------------------------------------------------------------------
// JSON helpers
// ----------------------------------------------------------------------

Json::Value ref(const std::string& kind, const std::string& name)
{
  Json::Value ret;
  ret["$ref"] = "#/components/" + kind + "/" + name;
  return ret;
}

Json::Value stringSchema(const std::string& description)
{
  Json::Value ret;
  ret["type"] = "string";
  if (!description.empty())
    ret["description"] = description;
  return ret;
}

Json::Value enumSchema(const std::vector<std::string>& values,
                       std::size_t maxEnum,
                       const std::string& valuesUrl)
{
  Json::Value ret;
  ret["type"] = "string";
  if (values.size() <= maxEnum)
  {
    Json::Value list(Json::arrayValue);
    for (const auto& value : values)
      list.append(value);
    ret["enum"] = list;
  }
  else
  {
    ret["description"] = fmt::format("One of {} values, too many to list here{}",
                                     values.size(),
                                     valuesUrl.empty() ? std::string() : ", see " + valuesUrl);
    if (!valuesUrl.empty())
      ret["x-values-url"] = valuesUrl;
  }
  return ret;
}

Json::Value arraySchema(const Json::Value& items)
{
  Json::Value ret;
  ret["type"] = "array";
  ret["items"] = items;
  return ret;
}

Json::Value parameter(const std::string& name,
                      const std::string& in,
                      const std::string& description,
                      const Json::Value& schema,
                      bool required)
{
  Json::Value ret;
  ret["name"] = name;
  ret["in"] = in;
  if (!description.empty())
    ret["description"] = description;
  // Path parameters are always required
  if (required || in == "path")
    ret["required"] = true;
  ret["schema"] = schema;
  if (in == "query" && schema.get("type", "").asString() == "array")
  {
    ret["style"] = "form";
    ret["explode"] = false;
  }
  return ret;
}

Json::Value link(const std::string& href,
                 const std::string& rel,
                 const std::string& type,
                 const std::string& title)
{
  Json::Value ret;
  ret["href"] = href;
  ret["rel"] = rel;
  if (!type.empty())
    ret["type"] = type;
  if (!title.empty())
    ret["title"] = title;
  return ret;
}

std::vector<std::string> documentationFiles()
{
  std::vector<std::string> ret;
  ret.reserve(swagger_ui_files.size());
  for (const auto& file : swagger_ui_files)
    ret.push_back(file.first);
  ret.emplace_back(PLUGIN_FILE);
  return ret;
}

Json::Value serviceDescLink(const std::string& root)
{
  return link(root + "/api", REL_SERVICE_DESC, MEDIA_TYPE, "The API definition (OpenAPI 3.1)");
}

Json::Value serviceDocLink(const std::string& root)
{
  return link(root + "/api/docs", REL_SERVICE_DOC, "text/html", "The API documentation");
}

// ----------------------------------------------------------------------
// Document
// ----------------------------------------------------------------------

Document::Document(const std::string& title,
                   const std::string& description,
                   const std::string& version,
                   const Settings& settings)
{
  itsDoc["openapi"] = OPENAPI_VERSION;
  auto& info = itsDoc["info"];
  info["title"] = title;
  if (!description.empty())
    info["description"] = description;
  info["version"] = version;
  if (!settings.contact.isNull())
    info["contact"] = settings.contact;
  if (!settings.license.isNull())
    info["license"] = settings.license;

  Json::Value server;
  server["url"] = SERVER_URL_PLACEHOLDER;
  itsDoc["servers"].append(server);

  // The background map of the location maps of the documentation page
  if (!settings.map_tiles.empty())
  {
    auto& map = itsDoc["x-smartmet-map"];
    map["tiles"] = settings.map_tiles;
    map["tileSize"] = settings.map_tile_size;
    if (!settings.map_attribution.empty())
      map["attribution"] = settings.map_attribution;
  }

  itsDoc["tags"] = Json::Value(Json::arrayValue);
  itsDoc["paths"] = Json::Value(Json::objectValue);
  itsDoc["components"] = Json::Value(Json::objectValue);
}

void Document::addTag(const std::string& name, const std::string& description)
{
  for (const auto& tag : itsDoc["tags"])
    if (tag["name"].asString() == name)
      return;
  Json::Value tag;
  tag["name"] = name;
  if (!description.empty())
    tag["description"] = description;
  itsDoc["tags"].append(tag);
}

Json::Value& Document::operation(const std::string& path, const std::string& method)
{
  auto& op = itsDoc["paths"][path][method];
  if (op.isNull())
    op = Json::Value(Json::objectValue);
  return op;
}

void Document::addComponent(const std::string& kind,
                            const std::string& name,
                            const Json::Value& value)
{
  itsDoc["components"][kind][name] = value;
}

bool Document::hasComponent(const std::string& kind, const std::string& name) const
{
  const auto& components = itsDoc["components"];
  return components.isMember(kind) && components[kind].isMember(name);
}

void Document::addCommonComponents()
{
  try
  {
    // Parameters

    Json::Value number;
    number["type"] = "number";
    auto bbox_schema = arraySchema(number);
    bbox_schema["minItems"] = 4;
    bbox_schema["maxItems"] = 6;
    addComponent("parameters",
                 "bbox",
                 parameter("bbox",
                           "query",
                           "Only data that has a geometry intersecting the bounding box is "
                           "selected: lower left corner and upper right corner, with optional "
                           "minimum and maximum heights (minx,miny,maxx,maxy or "
                           "minx,miny,minz,maxx,maxy,maxz).",
                           bbox_schema));

    Json::Value uri = stringSchema();
    uri["format"] = "uri";
    addComponent("parameters",
                 "bbox-crs",
                 parameter("bbox-crs", "query", "The coordinate reference system of bbox.", uri));

    addComponent("parameters",
                 "datetime",
                 parameter("datetime",
                           "query",
                           "A date-time or an interval (RFC 3339). Open intervals use '..' as "
                           "the open end, e.g. '2026-10-09T00:00:00Z/..'.",
                           stringSchema()));

    Json::Value limit;
    limit["type"] = "integer";
    limit["minimum"] = 1;
    addComponent("parameters",
                 "limit",
                 parameter("limit", "query", "The maximum number of items to return.", limit));

    addComponent(
        "parameters",
        "crs",
        parameter(
            "crs", "query", "The coordinate reference system of the response.", stringSchema()));

    // Schemas

    Json::Value link_schema;
    link_schema["type"] = "object";
    link_schema["required"].append("href");
    link_schema["required"].append("rel");
    auto& lp = link_schema["properties"];
    lp["href"] = stringSchema();
    lp["rel"] = stringSchema();
    lp["type"] = stringSchema();
    lp["hreflang"] = stringSchema();
    lp["title"] = stringSchema();
    lp["length"]["type"] = "integer";
    lp["templated"]["type"] = "boolean";
    addComponent("schemas", "link", link_schema);

    Json::Value landing;
    landing["type"] = "object";
    landing["required"].append("links");
    auto& pp = landing["properties"];
    pp["title"] = stringSchema();
    pp["description"] = stringSchema();
    pp["attribution"] = stringSchema();
    pp["links"] = arraySchema(ref("schemas", "link"));
    addComponent("schemas", "landingPage", landing);

    Json::Value conf;
    conf["type"] = "object";
    conf["required"].append("conformsTo");
    conf["properties"]["conformsTo"] = arraySchema(stringSchema());
    addComponent("schemas", "confClasses", conf);

    // RFC 7807 problem details. No member is required: the APIs predate the
    // OGC requirement and do not all send 'type'.
    Json::Value exception;
    exception["type"] = "object";
    auto& ep = exception["properties"];
    ep["type"] = stringSchema();
    ep["title"] = stringSchema();
    ep["status"]["type"] = "integer";
    ep["detail"] = stringSchema();
    ep["instance"] = stringSchema();
    ep["code"] = stringSchema();
    ep["description"] = stringSchema();
    addComponent("schemas", "exception", exception);

    // Responses

    auto json_response = [](const std::string& description, const Json::Value& schema)
    {
      Json::Value ret;
      ret["description"] = description;
      ret["content"]["application/json"]["schema"] = schema;
      return ret;
    };

    auto error_response = [](const std::string& description)
    {
      Json::Value ret;
      ret["description"] = description;
      ret["content"]["application/problem+json"]["schema"] = ref("schemas", "exception");
      ret["content"]["application/json"]["schema"] = ref("schemas", "exception");
      return ret;
    };

    addComponent("responses",
                 "LandingPage",
                 json_response("The landing page links to the API definition, the conformance "
                               "declaration and the data.",
                               ref("schemas", "landingPage")));
    addComponent("responses",
                 "ConformanceDeclaration",
                 json_response("The conformance classes the API conforms to.",
                               ref("schemas", "confClasses")));
    addComponent("responses", "BadRequest", error_response("The request is not valid."));
    addComponent("responses", "NotFound", error_response("The resource does not exist."));
    addComponent("responses",
                 "NotAcceptable",
                 error_response("None of the requested media types is supported."));
    addComponent("responses", "ServerError", error_response("A server error occurred."));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to add the OGC API - Common components");
  }
}

void Document::addErrorResponses(Json::Value& operation, bool notFound)
{
  auto& responses = operation["responses"];
  responses["400"] = ref("responses", "BadRequest");
  if (notFound)
    responses["404"] = ref("responses", "NotFound");
  responses["500"] = ref("responses", "ServerError");
}

void Document::addCoreOperations(const std::string& tag, const Json::Value& formatParameter)
{
  try
  {
    if (!hasComponent("responses", "LandingPage"))
      addCommonComponents();

    auto& landing = operation("/", "get");
    landing["tags"].append(tag);
    landing["summary"] = "Landing page";
    landing["description"] =
        "Links to the API definition, the documentation, the conformance declaration and the "
        "data.";
    landing["operationId"] = "getLandingPage";
    if (!formatParameter.isNull())
      landing["parameters"].append(formatParameter);
    landing["responses"]["200"] = ref("responses", "LandingPage");
    addErrorResponses(landing, false);

    auto& conformance = operation("/conformance", "get");
    conformance["tags"].append(tag);
    conformance["summary"] = "Conformance declaration";
    conformance["description"] = "The conformance classes the API conforms to.";
    conformance["operationId"] = "getConformance";
    if (!formatParameter.isNull())
      conformance["parameters"].append(formatParameter);
    conformance["responses"]["200"] = ref("responses", "ConformanceDeclaration");
    addErrorResponses(conformance, false);

    addApiOperations(tag);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to add the core OpenAPI operations");
  }
}

void Document::addApiOperations(const std::string& tag, const std::string& prefix)
{
  try
  {
    if (!hasComponent("responses", "NotFound"))
      addCommonComponents();

    Json::Value f_schema = stringSchema();
    f_schema["enum"].append("json");
    f_schema["enum"].append("html");

    auto& api = operation(prefix + "/api", "get");
    api["tags"].append(tag);
    api["summary"] = "API definition";
    api["description"] =
        "This document. With f=html, or when the client prefers text/html, the documentation "
        "page is returned instead.";
    api["operationId"] = "getAPI";
    api["parameters"].append(parameter(
        "f", "query", "The format of the response: json (the default) or html.", f_schema));
    auto& api_ok = api["responses"]["200"];
    api_ok["description"] = "The OpenAPI 3.1 definition of the API.";
    Json::Value object_schema;
    object_schema["type"] = "object";
    api_ok["content"][MEDIA_TYPE]["schema"] = object_schema;
    api_ok["content"]["text/html"]["schema"] = stringSchema();
    addErrorResponses(api, false);

    auto& docs = operation(prefix + "/api/docs", "get");
    docs["tags"].append(tag);
    docs["summary"] = "API documentation";
    docs["description"] = "Human-readable documentation of the API, generated from its definition.";
    docs["operationId"] = "getAPIDocs";
    auto& docs_ok = docs["responses"]["200"];
    docs_ok["description"] = "The documentation page.";
    docs_ok["content"]["text/html"]["schema"] = stringSchema();
    docs["responses"]["404"] = ref("responses", "NotFound");

    auto& files = operation(prefix + "/api/docs/{file}", "get");
    files["tags"].append(tag);
    files["summary"] = "Files of the documentation page";
    files["operationId"] = "getAPIDocsFile";
    files["parameters"].append(
        parameter("file",
                  "path",
                  "File name",
                  enumSchema(documentationFiles(), documentationFiles().size())));
    auto& files_ok = files["responses"]["200"];
    files_ok["description"] = "The file";
    files["responses"]["404"] = ref("responses", "NotFound");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to add the API definition operations");
  }
}

// ----------------------------------------------------------------------
// Definition
// ----------------------------------------------------------------------

Definition::Definition(const Document& doc)
{
  try
  {
    const auto text = toJsonText(doc.json());
    const std::string placeholder = std::string("\"") + SERVER_URL_PLACEHOLDER + "\"";
    const auto pos = text.find(placeholder);
    if (pos == std::string::npos)
      throw Fmi::Exception(BCP, "OpenAPI definition has no server URL placeholder");

    // Keep the quotes in the prefix and suffix
    itsPrefix = text.substr(0, pos + 1);
    itsSuffix = text.substr(pos + placeholder.size() - 1);
    itsTitle = doc.json()["info"]["title"].asString();
    itsHash = Fmi::hash_value(text);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to serialize the OpenAPI definition");
  }
}

std::string Definition::json(const std::string& serverUrl) const
{
  const auto url = jsonEscape(serverUrl);
  std::string ret;
  ret.reserve(itsPrefix.size() + url.size() + itsSuffix.size());
  ret += itsPrefix;
  ret += url;
  ret += itsSuffix;
  return ret;
}

// ----------------------------------------------------------------------
// Publisher
// ----------------------------------------------------------------------

Publisher::Publisher(Settings settings) : itsSettings(std::move(settings))
{
  if (!itsSettings.enabled || !itsSettings.ui)
    return;

  std::string missing;
  for (const auto& file : swagger_ui_files)
  {
    const auto path = itsSettings.swagger_ui_dir + "/" + file.first;
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
      missing += (missing.empty() ? "" : ", ") + file.first;
      continue;
    }
    Asset asset;
    asset.content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    asset.contentType = file.second;
    asset.etag = quotedETag(Fmi::hash_value(asset.content), "asset");
    itsAssets.emplace(file.first, std::move(asset));
  }

  itsSwaggerUi = missing.empty();

  // The SmartMet plugin is built in
  Asset snippets;
  snippets.content = SWAGGER_UI_PLUGIN_JS;
  snippets.contentType = "text/javascript; charset=UTF-8";
  snippets.etag = quotedETag(Fmi::hash_value(snippets.content), "asset");
  itsAssets.emplace(PLUGIN_FILE, std::move(snippets));

  if (!missing.empty())
    std::cerr << fmt::format(
        "{} {}Warning:{} OpenAPI documentation pages are unavailable, Swagger UI files missing "
        "from {}: {} (package smartmet-swagger-ui)\n",
        log_time_str(),
        ANSI_FG_YELLOW,
        ANSI_FG_DEFAULT,
        itsSettings.swagger_ui_dir,
        missing);
}

Publisher::~Publisher()
{
  auto& registry = catalogRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  registry.entries.erase(this);
}

void Publisher::addToCatalog(const std::string& rootPath,
                             const std::string& title,
                             const std::string& description)
{
  addToCatalog(rootPath, title, description, "");
}

void Publisher::addToCatalog(const std::string& rootPath,
                             const std::string& title,
                             const std::string& description,
                             const std::string& kind)
{
  try
  {
    if (!itsSettings.enabled)
      return;

    std::string root = rootPath;
    while (!root.empty() && root.back() == '/')
      root.pop_back();
    if (!validRootPath(root))
      throw Fmi::Exception(BCP, "Invalid API root path for the API catalog")
          .addParameter("Path", rootPath);

    auto& registry = catalogRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto& registration = registry.entries[this];
    registration.order = ++registry.counter;
    registration.entry.root = root;
    registration.entry.title = title;
    registration.entry.description = description;
    registration.entry.kind = kind;
    registration.entry.docs = (itsSettings.ui && itsSwaggerUi);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to add an API to the API catalog");
  }
}

void Publisher::publish(const Document& doc)
{
  auto definition = std::make_shared<const Definition>(doc);
  std::atomic_store(&itsDefinition, std::move(definition));
}

std::shared_ptr<const Definition> Publisher::definition() const
{
  return std::atomic_load(&itsDefinition);
}

bool Publisher::isApiResource(const std::string& resource, const std::string& rootPath)
{
  std::string root = rootPath;
  while (!root.empty() && root.back() == '/')
    root.pop_back();

  const auto api = root + "/api";
  if (!boost::algorithm::starts_with(resource, api))
    return false;
  const auto rest = resource.substr(api.size());
  return (rest.empty() || rest == "/" || rest == "/docs" ||
          boost::algorithm::starts_with(rest, "/docs/"));
}

bool Publisher::handle(const HTTP::Request& request,
                       HTTP::Response& response,
                       const std::string& rootPath,
                       const std::string& serverUrl) const
{
  return handle(request, response, rootPath, serverUrl, definition());
}

bool Publisher::handle(const HTTP::Request& request,
                       HTTP::Response& response,
                       const std::string& rootPath,
                       const std::string& serverUrl,
                       const std::shared_ptr<const Definition>& definition) const
{
  try
  {
    if (!itsSettings.enabled)
      return false;

    const auto& resource = request.getResource();
    if (!isApiResource(resource, rootPath))
      return false;

    std::string root = rootPath;
    while (!root.empty() && root.back() == '/')
      root.pop_back();

    const auto api = root + "/api";
    const auto docs = api + "/docs";
    const auto rest = resource.substr(api.size());

    if (rest.empty() || rest == "/")
    {
      if (prefersHtml(request) && itsSettings.ui)
        handleDocs(request,
                   response,
                   relativeHref(resource, api),
                   relativeHref(resource, docs + "/"),
                   definition);
      else
        handleDefinition(request, response, serverUrl, definition);
    }
    else if (rest == "/docs" || rest == "/docs/")
    {
      if (!itsSettings.ui)
        problem(response, 404, "Not Found", "API documentation is disabled");
      else
        handleDocs(request,
                   response,
                   relativeHref(resource, api),
                   relativeHref(resource, docs + "/"),
                   definition);
    }
    else
    {
      handleAsset(request, response, rest.substr(6));
    }
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to serve the OpenAPI resource");
  }
}

void Publisher::handleDefinition(const HTTP::Request& request,
                                 HTTP::Response& response,
                                 const std::string& serverUrl,
                                 const std::shared_ptr<const Definition>& definition) const
{
  const auto f = request.getParameter("f");
  if (f && *f != "json" && *f != "html")
  {
    problem(response, 400, "Bad Request", "Unsupported format '" + *f + "', use json or html");
    return;
  }
  if (f && *f == "html")
  {
    problem(response, 404, "Not Found", "API documentation is disabled");
    return;
  }

  if (!definition)
  {
    problem(response, 503, "Service Unavailable", "The API definition is not available yet");
    return;
  }

  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Vary", "Accept");
  response.setHeader("Cache-Control", cacheControl(request));

  std::size_t hash = definition->hash();
  Fmi::hash_combine(hash, Fmi::hash_value(serverUrl));
  if (etagOnly(request, response, quotedETag(hash, "json")))
    return;

  response.setStatus(HTTP::Status::ok);
  response.setHeader("Content-Type", MEDIA_TYPE);
  response.setContent(definition->json(serverUrl));
}

void Publisher::handleDocs(const HTTP::Request& request,
                           HTTP::Response& response,
                           const std::string& definitionHref,
                           const std::string& assetPrefix,
                           const std::shared_ptr<const Definition>& definition) const
{
  if (!itsSwaggerUi)
  {
    problem(response,
            503,
            "Service Unavailable",
            "The API documentation is not available: Swagger UI is not installed");
    return;
  }

  const std::string title = (definition ? definition->title() : std::string("API"));

  const auto a = htmlEscape(assetPrefix);
  const auto html = fmt::format(
      R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{0} - API documentation</title>
<link rel="icon" type="image/png" href="{1}favicon-32x32.png" sizes="32x32">
<link rel="icon" type="image/png" href="{1}favicon-16x16.png" sizes="16x16">
<link rel="stylesheet" href="{1}swagger-ui.css">
<style>body {{ margin: 0; }}</style>
</head>
<body>
<div id="swagger-ui"></div>
<script src="{1}swagger-ui-bundle.js"></script>
<script src="{1}smartmet-swagger-ui.js"></script>
<script>
window.onload = function () {{
  window.ui = SwaggerUIBundle({{
    url: new URL("{2}", window.location.href).href,
    dom_id: "#swagger-ui",
    deepLinking: true,
    validatorUrl: null,
    queryConfigEnabled: false,
    tryItOutEnabled: true,
    requestSnippetsEnabled: true,
    requestSnippets: SmartMetSwaggerUI.snippetConfig,
    presets: [SwaggerUIBundle.presets.apis],
    plugins: [SmartMetSwaggerUI.plugin],
    layout: "BaseLayout"
  }});
}};
</script>
</body>
</html>
)HTML",
      htmlEscape(title),
      a,
      jsonEscape(definitionHref));

  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Vary", "Accept");
  response.setHeader("Cache-Control", DEFINITION_CACHE_CONTROL);
  if (etagOnly(request, response, quotedETag(Fmi::hash_value(html), "html")))
    return;

  response.setStatus(HTTP::Status::ok);
  response.setHeader("Content-Type", "text/html; charset=UTF-8");
  response.setHeader("Content-Security-Policy", docsPolicy(itsSettings));
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setContent(html);
}

void Publisher::handleAsset(const HTTP::Request& request,
                            HTTP::Response& response,
                            const std::string& name) const
{
  auto it = itsAssets.find(name);
  if (it == itsAssets.end())
  {
    problem(response, 404, "Not Found", "No such documentation file");
    return;
  }

  const auto& asset = it->second;
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Cache-Control", ASSET_CACHE_CONTROL);
  if (etagOnly(request, response, asset.etag))
    return;

  response.setStatus(HTTP::Status::ok);
  response.setHeader("Content-Type", asset.contentType);
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setContent(asset.content);
}

// ----------------------------------------------------------------------
// Request helpers
// ----------------------------------------------------------------------

std::string serverUrl(const HTTP::Request& request, const std::string& rootPath)
{
  try
  {
    std::string root = rootPath;
    while (!root.empty() && root.back() == '/')
      root.pop_back();

    auto host = request.getHeader("Host");
    if (!host || !validHost(*host))
      return root;

    // X-Forwarded-Proto is client-controlled: accept the two valid schemes only
    auto protocol = request.getProtocol();
    std::string scheme = (protocol ? *protocol : "http");
    if (scheme != "http" && scheme != "https")
      scheme = "http";

    std::string apikey_path;
    const bool check_token = false;
    auto apikey = FmiApiKey::getFmiApiKey(request, check_token);
    if (apikey && validApiKey(*apikey) && FmiApiKey::shouldReturnApiKey(request))
      apikey_path = "/fmi-apikey/" + *apikey;

    return scheme + "://" + *host + apikey_path + root;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to resolve the API server URL");
  }
}

bool prefersHtml(const HTTP::Request& request)
{
  auto f = request.getParameter("f");
  if (f)
    return (*f == "html");

  auto accept = request.getHeader("Accept");
  if (!accept)
    return false;

  double html_q = 0;
  double json_q = 0;

  std::vector<std::string> ranges;
  boost::algorithm::split(ranges, *accept, boost::algorithm::is_any_of(","));
  for (const auto& range : ranges)
  {
    std::vector<std::string> parts;
    boost::algorithm::split(parts, range, boost::algorithm::is_any_of(";"));
    const auto type = boost::algorithm::to_lower_copy(Fmi::trim_copy(parts[0]));
    const double q = qualityValue(parts);

    if (type == "text/html" || type == "application/xhtml+xml")
      html_q = std::max(html_q, q);
    else if (type == "application/vnd.oai.openapi+json" || type == "application/json" ||
             type == "application/*" || type == "*/*" || type == "application/yaml" ||
             type == "application/vnd.oai.openapi")
      json_q = std::max(json_q, q);
  }

  return (html_q > json_q);
}

// ----------------------------------------------------------------------
// The API catalog
// ----------------------------------------------------------------------

CatalogSettings CatalogSettings::parse(const libconfig::Config& config, const std::string& path)
{
  try
  {
    CatalogSettings settings;
    settings.openapi = Settings::parse(config, path);
    if (config.exists(path))
    {
      const auto& group = config.lookup(path);
      group.lookupValue("title", settings.title);
      group.lookupValue("description", settings.description);
      group.lookupValue("explorer", settings.explorer);
      group.lookupValue("explorer_dir", settings.explorer_dir);
      group.lookupValue("uplot_dir", settings.uplot_dir);
    }
    return settings;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse the API catalog settings");
  }
}

std::vector<CatalogEntry> localCatalogEntries()
{
  try
  {
    // The latest registration of each root
    std::map<std::string, std::pair<std::uint64_t, CatalogEntry>> latest;
    {
      auto& registry = catalogRegistry();
      std::lock_guard<std::mutex> lock(registry.mutex);
      for (const auto& item : registry.entries)
      {
        auto entry = item.second.entry;
        if (entry.title.empty())
        {
          auto definition = item.first->definition();
          entry.title =
              (definition && !definition->title().empty() ? definition->title() : entry.root);
        }
        auto& current = latest[entry.root];
        if (item.second.order > current.first)
          current = std::make_pair(item.second.order, std::move(entry));
      }
    }

    std::vector<CatalogEntry> ret;
    ret.reserve(latest.size());
    for (auto& item : latest)
      ret.push_back(std::move(item.second.second));
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to list the API catalog");
  }
}

std::vector<CatalogEntry> parseCatalog(const Json::Value& linkset)
{
  try
  {
    std::vector<CatalogEntry> ret;
    if (!linkset.isObject())
      return ret;
    const auto& contexts = linkset["linkset"];
    if (!contexts.isArray())
      return ret;

    // The APIs with documentation pages
    std::set<std::string> documented;
    for (const auto& context : contexts)
    {
      if (!context.isObject() || !context["anchor"].isString() || !context["service-doc"].isArray())
        continue;
      const auto root = catalogPath(context["anchor"].asString());
      if (!root.empty())
        documented.insert(root);
    }

    std::set<std::string> seen;
    for (const auto& context : contexts)
    {
      if (!context.isObject() || !context["item"].isArray())
        continue;
      for (const auto& item : context["item"])
      {
        if (!item.isObject() || !item["href"].isString())
          continue;
        CatalogEntry entry;
        entry.root = catalogPath(item["href"].asString());
        if (entry.root.empty() || !seen.insert(entry.root).second)
          continue;
        entry.title = (item["title"].isString() ? item["title"].asString() : entry.root);
        if (item["description"].isString())
          entry.description = item["description"].asString();
        if (item["x-smartmet-kind"].isString())
          entry.kind = item["x-smartmet-kind"].asString();
        entry.docs = (documented.count(entry.root) > 0);
        ret.push_back(std::move(entry));
      }
    }
    std::sort(ret.begin(),
              ret.end(),
              [](const CatalogEntry& a, const CatalogEntry& b) { return a.root < b.root; });
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse an API catalog");
  }
}

Catalog::Catalog(CatalogSettings settings) : itsSettings(std::move(settings))
{
  if (itsSettings.openapi.enabled && itsSettings.openapi.ui)
    itsFiles = std::make_unique<Publisher>(itsSettings.openapi);

  if (!itsSettings.openapi.enabled || !itsSettings.explorer)
    return;

  // The explorer, uPlot, and the SmartMet Swagger UI plugin for the snippets and the viewer
  const std::vector<std::pair<std::string, std::string>> files{
      {itsSettings.explorer_dir + "/explorer.js", "text/javascript; charset=UTF-8"},
      {itsSettings.explorer_dir + "/explorer.css", "text/css; charset=UTF-8"},
      {itsSettings.uplot_dir + "/uPlot.iife.min.js", "text/javascript; charset=UTF-8"},
      {itsSettings.uplot_dir + "/uPlot.min.css", "text/css; charset=UTF-8"}};

  std::string missing;
  std::map<std::string, ExplorerFile> loaded;
  for (const auto& file : files)
  {
    std::ifstream in(file.first, std::ios::binary);
    if (!in)
    {
      missing += (missing.empty() ? "" : ", ") + file.first;
      continue;
    }
    ExplorerFile f;
    f.content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    f.contentType = file.second;
    f.etag = quotedETag(Fmi::hash_value(f.content), "explorer");
    loaded.emplace(file.first.substr(file.first.rfind('/') + 1), std::move(f));
  }

  if (!missing.empty())
  {
    std::cerr << fmt::format(
        "{} {}Warning:{} The data explorer is unavailable, files missing: {} (packages "
        "smartmet-library-spine and smartmet-uplot)\n",
        log_time_str(),
        ANSI_FG_YELLOW,
        ANSI_FG_DEFAULT,
        missing);
    return;
  }

  ExplorerFile plugin;
  plugin.content = SWAGGER_UI_PLUGIN_JS;
  plugin.contentType = "text/javascript; charset=UTF-8";
  plugin.etag = quotedETag(Fmi::hash_value(plugin.content), "explorer");
  loaded.emplace(PLUGIN_FILE, std::move(plugin));
  itsExplorerFiles = std::move(loaded);
}

Catalog::~Catalog() = default;

bool Catalog::isCatalogResource(const std::string& resource)
{
  return (resource == "/.well-known/api-catalog" || resource == "/api" || resource == "/api/" ||
          resource == "/api/docs" || resource == "/api/docs/" ||
          boost::algorithm::starts_with(resource, "/api/docs/") || resource == "/api/explorer" ||
          resource == "/api/explorer/" ||
          boost::algorithm::starts_with(resource, "/api/explorer/"));
}

Json::Value Catalog::linkset(const HTTP::Request& request, const std::vector<CatalogEntry>& entries)
{
  try
  {
    const auto base = serverUrl(request, "");

    Json::Value catalog;
    catalog["anchor"] = base + "/.well-known/api-catalog";
    catalog["item"] = Json::Value(Json::arrayValue);

    Json::Value contexts(Json::arrayValue);
    for (const auto& entry : entries)
    {
      const auto root = base + entry.root;

      Json::Value item;
      item["href"] = root;
      item["title"] = entry.title;
      if (!entry.description.empty())
        item["description"] = entry.description;
      if (!entry.kind.empty())
        item["x-smartmet-kind"] = entry.kind;
      catalog["item"].append(item);

      Json::Value context;
      context["anchor"] = root;
      Json::Value desc;
      desc["href"] = root + "/api";
      desc["type"] = MEDIA_TYPE;
      desc["title"] = entry.title + " - OpenAPI definition";
      context[REL_SERVICE_DESC].append(desc);
      if (entry.docs)
      {
        Json::Value doc;
        doc["href"] = root + "/api/docs";
        doc["type"] = "text/html";
        doc["title"] = entry.title + " - API documentation";
        context[REL_SERVICE_DOC].append(doc);
      }
      contexts.append(context);
    }

    Json::Value ret;
    ret["linkset"].append(catalog);
    for (const auto& context : contexts)
      ret["linkset"].append(context);
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to build the API catalog");
  }
}

bool Catalog::handle(const HTTP::Request& request,
                     HTTP::Response& response,
                     const std::vector<CatalogEntry>& entries) const
{
  try
  {
    if (!itsSettings.openapi.enabled)
      return false;

    const auto& resource = request.getResource();
    if (!isCatalogResource(resource))
      return false;

    // Every response links to the catalog, which is how RFC 9727 answers HEAD requests
    response.setHeader(
        "Link", "<" + serverUrl(request, "") + "/.well-known/api-catalog>; rel=\"api-catalog\"");

    const bool ui = (itsSettings.openapi.ui && itsFiles);

    if (resource == "/api/explorer" || resource == "/api/explorer/")
    {
      handleExplorer(request, response);
      return true;
    }
    if (boost::algorithm::starts_with(resource, "/api/explorer/"))
    {
      handleExplorerFile(request, response, resource.substr(14));
      return true;
    }

    if (resource == "/.well-known/api-catalog")
    {
      handleLinkset(request, response, entries);
    }
    else if (resource == "/api" || resource == "/api/")
    {
      const auto f = request.getParameter("f");
      if (f && *f != "json" && *f != "html")
        problem(response, 400, "Bad Request", "Unsupported format '" + *f + "', use json or html");
      else if (prefersHtml(request))
        handlePage(request, response, entries);
      else
        handleLinkset(request, response, entries);
    }
    else if (!ui)
    {
      problem(response, 404, "Not Found", "API documentation is disabled");
    }
    else if (resource == "/api/docs" || resource == "/api/docs/")
    {
      handleDocs(request, response, entries);
    }
    else
    {
      itsFiles->handleAsset(request, response, resource.substr(10));
    }
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to serve the API catalog");
  }
}

void Catalog::handleLinkset(const HTTP::Request& request,
                            HTTP::Response& response,
                            const std::vector<CatalogEntry>& entries) const
{
  const auto text = toJsonText(linkset(request, entries));

  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Vary", "Accept");
  response.setHeader("Cache-Control", cacheControl(request));
  if (etagOnly(request, response, quotedETag(Fmi::hash_value(text), "catalog")))
    return;

  response.setStatus(HTTP::Status::ok);
  response.setHeader("Content-Type", CATALOG_MEDIA_TYPE);
  response.setContent(text);
}

void Catalog::handlePage(const HTTP::Request& request,
                         HTTP::Response& response,
                         const std::vector<CatalogEntry>& entries) const
{
  const auto& resource = request.getResource();
  const bool ui = (itsSettings.openapi.ui && itsFiles);

  std::string rows;
  for (const auto& entry : entries)
  {
    const auto definition = htmlEscape(relativeHref(resource, entry.root + "/api"));
    const auto docs = htmlEscape(relativeHref(resource, entry.root + "/api/docs"));
    rows += fmt::format(
        "<tr><td><a href=\"{}\">{}</a><div class=\"root\">{}</div></td><td>{}</td>"
        "<td class=\"links\"><a href=\"{}\">OpenAPI definition</a>{}</td></tr>\n",
        entry.docs ? docs : definition,
        htmlEscape(entry.title),
        htmlEscape(entry.root),
        htmlEscape(entry.description),
        definition,
        entry.docs ? fmt::format("<br><a href=\"{}\">Documentation</a>", docs) : "");
  }
  if (entries.empty())
    rows = "<tr><td colspan=\"3\">No APIs are available.</td></tr>\n";

  std::string links = fmt::format("<a href=\"{}\">API catalog (RFC 9727 linkset)</a>",
                                  htmlEscape(relativeHref(resource, "/.well-known/api-catalog")));
  if (ui && !entries.empty())
    links = fmt::format("<a href=\"{}\">All APIs in one documentation page</a> · {}",
                        htmlEscape(relativeHref(resource, "/api/docs")),
                        links);
  if (!itsExplorerFiles.empty() && !entries.empty())
    links = fmt::format("<a href=\"{}\">Data explorer</a> · {}",
                        htmlEscape(relativeHref(resource, "/api/explorer")),
                        links);

  const auto html = fmt::format(
      R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{0}</title>
<style>
:root {{ color-scheme: light dark; --fg: #1b1f23; --muted: #57606a; --line: #d0d7de; --bg: #ffffff; --head: #f6f8fa; --link: #0969da; }}
@media (prefers-color-scheme: dark) {{ :root {{ --fg: #e6edf3; --muted: #9198a1; --line: #3d444d; --bg: #0d1117; --head: #151b23; --link: #4493f8; }} }}
body {{ margin: 0; padding: 24px 16px; font: 15px/1.5 system-ui, sans-serif; color: var(--fg); background: var(--bg); }}
main {{ max-width: 960px; margin: 0 auto; }}
h1 {{ font-size: 1.6em; margin: 0 0 4px; }}
p {{ color: var(--muted); margin: 0 0 16px; }}
a {{ color: var(--link); }}
table {{ width: 100%; border-collapse: collapse; margin: 16px 0; }}
th, td {{ text-align: left; vertical-align: top; padding: 8px 10px; border-bottom: 1px solid var(--line); }}
th {{ background: var(--head); font-weight: 600; }}
.root {{ font-family: ui-monospace, monospace; font-size: 0.85em; color: var(--muted); }}
.links {{ white-space: nowrap; }}
@media (max-width: 600px) {{ .links {{ white-space: normal; }} th, td {{ padding: 6px; }} }}
</style>
</head>
<body>
<main>
<h1>{0}</h1>
<p>{1}</p>
<table>
<thead><tr><th>API</th><th>Description</th><th>Links</th></tr></thead>
<tbody>
{2}</tbody>
</table>
<p>{3}</p>
</main>
</body>
</html>
)HTML",
      htmlEscape(itsSettings.title),
      htmlEscape(itsSettings.description),
      rows,
      links);

  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Vary", "Accept");
  response.setHeader("Cache-Control", cacheControl(request));
  if (etagOnly(request, response, quotedETag(Fmi::hash_value(html), "cataloghtml")))
    return;

  response.setStatus(HTTP::Status::ok);
  response.setHeader("Content-Type", "text/html; charset=UTF-8");
  response.setHeader("Content-Security-Policy",
                     "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; "
                     "form-action 'none'; frame-ancestors 'self'");
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setContent(html);
}

void Catalog::handleDocs(const HTTP::Request& request,
                         HTTP::Response& response,
                         const std::vector<CatalogEntry>& entries) const
{
  if (!itsFiles->itsSwaggerUi)
  {
    problem(response,
            503,
            "Service Unavailable",
            "The API documentation is not available: Swagger UI is not installed");
    return;
  }
  if (entries.empty())
  {
    problem(response, 404, "Not Found", "No APIs are available");
    return;
  }

  const auto& resource = request.getResource();

  // The definitions in the selector of the top bar
  Json::Value urls(Json::arrayValue);
  for (const auto& entry : entries)
  {
    Json::Value url;
    url["url"] = relativeHref(resource, entry.root + "/api");
    url["name"] = entry.title + " (" + entry.root + ")";
    urls.append(url);
  }
  // Escape '<' so that no text can close the script element
  auto urls_text = toJsonText(urls);
  boost::algorithm::replace_all(urls_text, "<", "\\u003c");

  const auto a = htmlEscape(relativeHref(resource, "/api/docs/"));
  const auto html = fmt::format(
      R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{0} - API documentation</title>
<link rel="icon" type="image/png" href="{1}favicon-32x32.png" sizes="32x32">
<link rel="icon" type="image/png" href="{1}favicon-16x16.png" sizes="16x16">
<link rel="stylesheet" href="{1}swagger-ui.css">
<style>body {{ margin: 0; }} .swagger-ui .topbar .link {{ display: none; }}</style>
</head>
<body>
<div id="swagger-ui"></div>
<script src="{1}swagger-ui-bundle.js"></script>
<script src="{1}swagger-ui-standalone-preset.js"></script>
<script src="{1}smartmet-swagger-ui.js"></script>
<script>
window.onload = function () {{
  var urls = {2};
  urls.forEach(function (u) {{ u.url = new URL(u.url, window.location.href).href; }});
  window.ui = SwaggerUIBundle({{
    urls: urls,
    dom_id: "#swagger-ui",
    deepLinking: true,
    validatorUrl: null,
    queryConfigEnabled: false,
    tryItOutEnabled: true,
    requestSnippetsEnabled: true,
    requestSnippets: SmartMetSwaggerUI.snippetConfig,
    presets: [SwaggerUIBundle.presets.apis, SwaggerUIStandalonePreset],
    plugins: [SmartMetSwaggerUI.plugin],
    layout: "StandaloneLayout"
  }});
}};
</script>
</body>
</html>
)HTML",
      htmlEscape(itsSettings.title),
      a,
      urls_text);

  response.setHeader("Access-Control-Allow-Origin", "*");
  response.setHeader("Cache-Control", cacheControl(request));
  if (etagOnly(request, response, quotedETag(Fmi::hash_value(html), "catalogdocs")))
    return;

  response.setStatus(HTTP::Status::ok);
  response.setHeader("Content-Type", "text/html; charset=UTF-8");
  response.setHeader("Content-Security-Policy", docsPolicy(itsSettings.openapi));
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setContent(html);
}

void Catalog::handleExplorer(const HTTP::Request& request, HTTP::Response& response) const
{
  if (!itsSettings.explorer)
  {
    problem(response, 404, "Not Found", "The data explorer is disabled");
    return;
  }
  if (itsExplorerFiles.empty())
  {
    problem(response,
            503,
            "Service Unavailable",
            "The data explorer is not available: its files are not installed");
    return;
  }

  const auto& resource = request.getResource();
  const auto a = htmlEscape(relativeHref(resource, "/api/explorer/"));
  const auto catalog = htmlEscape(relativeHref(resource, "/api"));

  // The background of the location maps
  std::string map;
  const auto& o = itsSettings.openapi;
  if (!o.map_tiles.empty())
    map = fmt::format(R"( data-map-tiles="{}" data-map-tile-size="{}" data-map-attribution="{}")",
                      htmlEscape(o.map_tiles),
                      o.map_tile_size,
                      htmlEscape(o.map_attribution));

  // The file hashes in the links, since the files are cached for a day
  const auto v = [this](const std::string& name)
  {
    const auto& etag = itsExplorerFiles.at(name).etag;
    return etag.substr(1, etag.find('-') - 1);
  };
  const auto html = fmt::format(
      R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{0} - Data explorer</title>
<link rel="stylesheet" href="{1}uPlot.min.css?v={3}">
<link rel="stylesheet" href="{1}explorer.css?v={4}">
</head>
<body>
<div id="explorer" data-catalog="{2}" data-title="{0}"{8}></div>
<script src="{1}uPlot.iife.min.js?v={5}"></script>
<script src="{1}smartmet-swagger-ui.js?v={6}"></script>
<script src="{1}explorer.js?v={7}"></script>
</body>
</html>
)HTML",
      htmlEscape(itsSettings.title),
      a,
      catalog,
      v("uPlot.min.css"),
      v("explorer.css"),
      v("uPlot.iife.min.js"),
      v(PLUGIN_FILE),
      v("explorer.js"),
      map);

  response.setHeader("Cache-Control", cacheControl(request));
  if (etagOnly(request, response, quotedETag(Fmi::hash_value(html), "explorerhtml")))
    return;

  response.setStatus(HTTP::Status::ok);
  response.setHeader("Content-Type", "text/html; charset=UTF-8");
  // No inline scripts, and data only from this server and the map tiles
  std::string policy =
      "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
      "img-src 'self' data:; connect-src 'self'; base-uri 'none'; form-action 'none'; "
      "frame-ancestors 'self'";
  const auto origin = tileOrigin(itsSettings.openapi.map_tiles);
  if (!origin.empty())
    boost::algorithm::replace_first(policy, "img-src 'self'", "img-src 'self' " + origin);
  response.setHeader("Content-Security-Policy", policy);
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setContent(html);
}

void Catalog::handleExplorerFile(const HTTP::Request& request,
                                 HTTP::Response& response,
                                 const std::string& name) const
{
  auto it = itsExplorerFiles.find(name);
  if (!itsSettings.explorer || it == itsExplorerFiles.end())
  {
    problem(response, 404, "Not Found", "No such explorer file");
    return;
  }
  const auto& file = it->second;
  response.setHeader("Cache-Control", ASSET_CACHE_CONTROL);
  if (etagOnly(request, response, file.etag))
    return;
  response.setStatus(HTTP::Status::ok);
  response.setHeader("Content-Type", file.contentType);
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setContent(file.content);
}

}  // namespace OpenAPI
}  // namespace Spine
}  // namespace SmartMet
