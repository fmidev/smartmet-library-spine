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
#include <atomic>
#include <fstream>
#include <iostream>
#include <iterator>
#include <libconfig.h++>
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

// The request snippet plugin, see OpenApiSnippets.cpp
extern const char* const SNIPPETS_JS;

namespace
{
// Placeholder for the server URL in the serialized definition
const char* const SERVER_URL_PLACEHOLDER = "__SMARTMET_OPENAPI_SERVER_URL__";

const char* const SNIPPETS_FILE = "smartmet-snippets.js";

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
  for (const auto& file : swagger_ui_files)
    ret.push_back(file.first);
  ret.emplace_back(SNIPPETS_FILE);
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

  // The request snippets are built in
  Asset snippets;
  snippets.content = SNIPPETS_JS;
  snippets.contentType = "text/javascript; charset=UTF-8";
  snippets.etag = quotedETag(Fmi::hash_value(snippets.content), "asset");
  itsAssets.emplace(SNIPPETS_FILE, std::move(snippets));

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

Publisher::~Publisher() = default;

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
<script src="{1}smartmet-snippets.js"></script>
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
    requestSnippets: SmartMetSnippets.snippetConfig,
    presets: [SwaggerUIBundle.presets.apis],
    plugins: [SmartMetSnippets.plugin],
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
  response.setHeader("Content-Security-Policy", DOCS_CONTENT_SECURITY_POLICY);
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

}  // namespace OpenAPI
}  // namespace Spine
}  // namespace SmartMet
