// ======================================================================
/*!
 * \file
 * \brief Regression tests for SmartMet::Spine::OpenAPI
 */
// ======================================================================

#include "HTTP.h"
#include "OpenApi.h"
#include <boost/test/included/unit_test.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <libconfig.h++>

using namespace boost::unit_test;
using namespace SmartMet::Spine;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "OpenAPI tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

namespace
{
HTTP::Request makeRequest(const std::string& resource,
                          const HTTP::HeaderMap& headers = {},
                          const HTTP::ParamMap& params = {})
{
  return HTTP::Request{headers, {}, "1.1", params, resource, HTTP::RequestMethod::GET, false};
}

Json::Value parse(const std::string& text)
{
  Json::Value ret;
  Json::CharReaderBuilder builder;
  std::string errors;
  std::istringstream in(text);
  if (!Json::parseFromStream(builder, in, &ret, &errors))
    throw std::runtime_error("Invalid JSON: " + errors);
  return ret;
}

std::string header(const HTTP::Response& response, const std::string& name)
{
  auto value = response.getHeader(name);
  return value ? *value : std::string();
}

// A directory with dummy Swagger UI files, removed at destruction
struct AssetDir
{
  std::filesystem::path path;
  AssetDir()
  {
    path = std::filesystem::temp_directory_path() / ("openapi-test-" + std::to_string(::getpid()));
    std::filesystem::create_directories(path);
    for (const char* name : {"swagger-ui-bundle.js",
                             "swagger-ui-standalone-preset.js",
                             "swagger-ui.css",
                             "favicon-16x16.png",
                             "favicon-32x32.png"})
      std::ofstream(path / name) << "content of " << name;
  }
  ~AssetDir() { std::filesystem::remove_all(path); }
};

OpenAPI::Document makeDocument(const OpenAPI::Settings& settings)
{
  OpenAPI::Document doc("Test API", "A test", "1.0", settings);
  doc.addTag("Capabilities", "Core resources");
  doc.addCommonComponents();
  doc.addCoreOperations("Capabilities", Json::Value());
  return doc;
}

std::shared_ptr<OpenAPI::Publisher> makePublisher(const std::string& assetDir,
                                                  const std::string& mapTiles = "")
{
  OpenAPI::Settings settings;
  settings.swagger_ui_dir = assetDir;
  settings.map_tiles = mapTiles;
  auto publisher = std::make_shared<OpenAPI::Publisher>(settings);
  publisher->publish(makeDocument(settings));
  return publisher;
}

}  // namespace

BOOST_AUTO_TEST_CASE(settings_defaults_and_parsing)
{
  libconfig::Config empty;
  auto defaults = OpenAPI::Settings::parse(empty);
  BOOST_TEST(defaults.enabled);
  BOOST_TEST(defaults.ui);
  BOOST_TEST(defaults.expand_collections);
  BOOST_TEST(defaults.max_enum == 1000U);
  BOOST_TEST(defaults.swagger_ui_dir == "/usr/share/smartmet/swagger-ui");
  BOOST_TEST(defaults.contact.isNull());

  libconfig::Config config;
  config.readString(
      "openapi: { enabled = false; ui = false; expand_collections = false; max_enum = 5; "
      "swagger_ui_dir = \"/tmp/x\"; contact = { name = \"FMI\"; email = \"a@b.fi\"; }; "
      "license = { name = \"MIT\"; url = \"https://opensource.org/licenses/MIT\"; }; };");
  auto settings = OpenAPI::Settings::parse(config);
  BOOST_TEST(!settings.enabled);
  BOOST_TEST(!settings.ui);
  BOOST_TEST(!settings.expand_collections);
  BOOST_TEST(settings.max_enum == 5U);
  BOOST_TEST(settings.swagger_ui_dir == "/tmp/x");
  BOOST_TEST(settings.contact["name"].asString() == "FMI");
  BOOST_TEST(settings.contact["email"].asString() == "a@b.fi");
  BOOST_TEST(!settings.contact.isMember("url"));
  BOOST_TEST(settings.license["name"].asString() == "MIT");

  libconfig::Config map;
  map.readString(
      "openapi: { map_tiles = \"/tiles/collections/x/tiles/EPSG:3857/{z}/{y}/{x}?f=png\"; "
      "map_tile_size = 512; map_attribution = \"FMI\"; };");
  auto map_settings = OpenAPI::Settings::parse(map);
  OpenAPI::Document map_doc("T", "", "1", map_settings);
  BOOST_TEST(map_doc.json()["x-smartmet-map"]["tiles"].asString() ==
             "/tiles/collections/x/tiles/EPSG:3857/{z}/{y}/{x}?f=png");
  BOOST_TEST(map_doc.json()["x-smartmet-map"]["tileSize"].asInt() == 512);
  BOOST_TEST(map_doc.json()["x-smartmet-map"]["attribution"].asString() == "FMI");
  BOOST_TEST(!OpenAPI::Document("T", "", "1", defaults).json().isMember("x-smartmet-map"));

  libconfig::Config bad;
  bad.readString("openapi: { max_enum = -1; };");
  BOOST_CHECK_THROW(OpenAPI::Settings::parse(bad), std::exception);

  // Tiles are a path or an http(s) URL with a plain host name
  for (const char* tiles : {"tiles/{z}/{y}/{x}",
                            "ftp://example.org/{z}/{y}/{x}",
                            "https://exa mple.org/{z}/{y}/{x}",
                            "https://example.org;script-src *"})
  {
    libconfig::Config c;
    c.readString(std::string("openapi: { map_tiles = \"") + tiles + "\"; };");
    BOOST_CHECK_THROW(OpenAPI::Settings::parse(c), std::exception);
  }
  libconfig::Config absolute;
  absolute.readString("openapi: { map_tiles = \"https://tiles.example.org:8443/{z}/{y}/{x}\"; };");
  BOOST_CHECK_NO_THROW(OpenAPI::Settings::parse(absolute));
  libconfig::Config small;
  small.readString("openapi: { map_tile_size = 32; };");
  BOOST_CHECK_THROW(OpenAPI::Settings::parse(small), std::exception);
}

BOOST_AUTO_TEST_CASE(enum_schema_is_capped)
{
  auto small = OpenAPI::enumSchema({"a", "b"}, 2);
  BOOST_TEST(small["enum"].size() == 2U);

  auto large = OpenAPI::enumSchema({"a", "b", "c"}, 2, "https://example.org/values");
  BOOST_TEST(!large.isMember("enum"));
  BOOST_TEST(large["x-values-url"].asString() == "https://example.org/values");
  BOOST_TEST(large["type"].asString() == "string");
}

BOOST_AUTO_TEST_CASE(array_query_parameters_are_comma_separated)
{
  auto p = OpenAPI::parameter(
      "parameter-name", "query", "names", OpenAPI::arraySchema(OpenAPI::stringSchema()));
  BOOST_TEST(p["style"].asString() == "form");
  BOOST_TEST(p["explode"].asBool() == false);
  BOOST_TEST(!p.isMember("required"));

  auto path = OpenAPI::parameter("collectionId", "path", "id", OpenAPI::stringSchema());
  BOOST_TEST(path["required"].asBool());
  BOOST_TEST(!path.isMember("style"));
}

BOOST_AUTO_TEST_CASE(definition_inserts_escaped_server_url)
{
  OpenAPI::Settings settings;
  OpenAPI::Definition definition(makeDocument(settings));

  auto doc = parse(definition.json("https://example.org/edr"));
  BOOST_TEST(doc["openapi"].asString() == "3.1.1");
  BOOST_TEST(doc["servers"][0]["url"].asString() == "https://example.org/edr");
  BOOST_TEST(doc["paths"].isMember("/"));
  BOOST_TEST(doc["paths"].isMember("/conformance"));
  BOOST_TEST(doc["paths"].isMember("/api"));
  BOOST_TEST(doc["paths"].isMember("/api/docs"));
  BOOST_TEST(doc["paths"]["/api/docs/{file}"]["get"]["parameters"][0]["schema"]["enum"].size() ==
             OpenAPI::documentationFiles().size());
  BOOST_TEST(doc["components"]["schemas"].isMember("landingPage"));

  // A hostile URL cannot break out of the JSON string
  const std::string evil = "http://x\"}, \"injected\": {\"a\": \"";
  auto doc2 = parse(definition.json(evil));
  BOOST_TEST(doc2["servers"][0]["url"].asString() == evil);
  BOOST_TEST(!doc2.isMember("injected"));
}

BOOST_AUTO_TEST_CASE(api_operations_with_prefix)
{
  OpenAPI::Settings settings;
  OpenAPI::Document doc("TimeSeries", "", "1.0", settings);
  doc.addApiOperations("Capabilities", "/timeseries");
  const auto& paths = doc.json()["paths"];
  BOOST_TEST(paths.isMember("/timeseries/api"));
  BOOST_TEST(paths.isMember("/timeseries/api/docs"));
  BOOST_TEST(paths.isMember("/timeseries/api/docs/{file}"));
  BOOST_TEST(!paths.isMember("/"));
  BOOST_TEST(!paths.isMember("/conformance"));
  BOOST_TEST(doc.hasComponent("responses", "NotFound"));
}

BOOST_AUTO_TEST_CASE(api_resources)
{
  BOOST_TEST(OpenAPI::Publisher::isApiResource("/edr/api", "/edr"));
  BOOST_TEST(OpenAPI::Publisher::isApiResource("/edr/api/", "/edr/"));
  BOOST_TEST(OpenAPI::Publisher::isApiResource("/edr/api/docs", "/edr"));
  BOOST_TEST(OpenAPI::Publisher::isApiResource("/edr/api/docs/swagger-ui.css", "/edr"));
  BOOST_TEST(!OpenAPI::Publisher::isApiResource("/edr/apix", "/edr"));
  BOOST_TEST(!OpenAPI::Publisher::isApiResource("/edr/collections", "/edr"));
  BOOST_TEST(!OpenAPI::Publisher::isApiResource("/edr/api/other", "/edr"));
}

BOOST_AUTO_TEST_CASE(definition_for_ogc_api_browser)
{
  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());

  // The Accept header OGC API Browser sends
  auto request = makeRequest(
      "/edr/api",
      {{"Accept",
        "application/vnd.oai.openapi+json, application/json;q=0.9, application/yaml;q=0.8, "
        "*/*;q=0.5"}});
  HTTP::Response response;
  BOOST_TEST(publisher->handle(request, response, "/edr", "https://example.org/edr"));
  BOOST_TEST(response.getStatus() == HTTP::Status::ok);
  BOOST_TEST(header(response, "Content-Type") == OpenAPI::MEDIA_TYPE);
  BOOST_TEST(header(response, "Access-Control-Allow-Origin") == "*");
  BOOST_TEST(!header(response, "ETag").empty());
  auto doc = parse(response.getContent());
  BOOST_TEST(doc["servers"][0]["url"].asString() == "https://example.org/edr");

  // The ETag depends on the server URL
  HTTP::Response response2;
  publisher->handle(request, response2, "/edr", "http://other.org/edr");
  BOOST_TEST(header(response, "ETag") != header(response2, "ETag"));
}

BOOST_AUTO_TEST_CASE(caller_supplied_definition)
{
  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());

  OpenAPI::Settings settings;
  OpenAPI::Document other("Other API", "", "2.0", settings);
  auto definition = std::make_shared<const OpenAPI::Definition>(other);

  HTTP::Response response;
  publisher->handle(makeRequest("/edr/api"), response, "/edr", "/edr", definition);
  BOOST_TEST(parse(response.getContent())["info"]["title"].asString() == "Other API");

  HTTP::Response html;
  publisher->handle(makeRequest("/edr/api/docs"), html, "/edr", "/edr", definition);
  BOOST_TEST(html.getContent().find("<title>Other API - API documentation</title>") !=
             std::string::npos);
}

BOOST_AUTO_TEST_CASE(apikey_definitions_are_private)
{
  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());

  HTTP::Response anonymous;
  publisher->handle(makeRequest("/edr/api"), anonymous, "/edr", "/edr");
  BOOST_TEST(header(anonymous, "Cache-Control") == "public, max-age=60");

  HTTP::Response with_key;
  publisher->handle(makeRequest("/edr/api", {{"fmi-apikey", "abc"}}), with_key, "/edr", "/edr");
  BOOST_TEST(header(with_key, "Cache-Control") == "private, max-age=60");
}

BOOST_AUTO_TEST_CASE(browsers_get_the_documentation_page)
{
  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());

  auto request = makeRequest(
      "/edr/api", {{"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"}});
  HTTP::Response response;
  publisher->handle(request, response, "/edr", "https://example.org/edr");
  BOOST_TEST(response.getStatus() == HTTP::Status::ok);
  BOOST_TEST(header(response, "Content-Type") == "text/html; charset=UTF-8");
  const auto html = response.getContent();
  BOOST_TEST(html.find("href=\"api/docs/swagger-ui.css\"") != std::string::npos);
  BOOST_TEST(html.find("new URL(\"api\"") != std::string::npos);
  BOOST_TEST(html.find("<title>Test API - API documentation</title>") != std::string::npos);
  BOOST_TEST(html.find("validatorUrl: null") != std::string::npos);
  BOOST_TEST(html.find("queryConfigEnabled: false") != std::string::npos);
  BOOST_TEST(header(response, "Content-Security-Policy").find("default-src 'none'") !=
             std::string::npos);
  BOOST_TEST(header(response, "X-Content-Type-Options") == "nosniff");
  BOOST_TEST(html.find("https://") == std::string::npos);  // no external resources
}

BOOST_AUTO_TEST_CASE(documentation_page_allows_the_tile_origin)
{
  AssetDir assets;
  const auto policy = [&](const std::string& tiles)
  {
    HTTP::Response response;
    makePublisher(assets.path.string(), tiles)
        ->handle(makeRequest("/edr/api/docs"), response, "/edr", "https://example.org/edr");
    return header(response, "Content-Security-Policy");
  };
  BOOST_TEST(policy("").find("img-src 'self' data:;") != std::string::npos);
  BOOST_TEST(policy("/tiles/{z}/{y}/{x}").find("img-src 'self' data:;") != std::string::npos);
  BOOST_TEST(policy("https://tiles.example.org:8443/{z}/{y}/{x}")
                 .find("img-src 'self' https://tiles.example.org:8443 data:;") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(documentation_page_relative_links)
{
  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());

  HTTP::Response r1;
  publisher->handle(makeRequest("/edr/api/docs"), r1, "/edr", "https://example.org/edr");
  BOOST_TEST(r1.getStatus() == HTTP::Status::ok);
  auto html = r1.getContent();
  BOOST_TEST(html.find("href=\"docs/swagger-ui.css\"") != std::string::npos);
  BOOST_TEST(html.find("new URL(\"../api\"") != std::string::npos);

  HTTP::Response r2;
  publisher->handle(makeRequest("/edr/api/docs/"), r2, "/edr", "https://example.org/edr");
  html = r2.getContent();
  BOOST_TEST(html.find("href=\"./swagger-ui.css\"") != std::string::npos);
  BOOST_TEST(html.find("new URL(\"../../api\"") != std::string::npos);

  HTTP::Response r3;
  publisher->handle(
      makeRequest("/timeseries/api", {}, {{"f", "html"}}), r3, "/timeseries", "/timeseries");
  html = r3.getContent();
  BOOST_TEST(html.find("href=\"api/docs/swagger-ui.css\"") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(assets)
{
  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());

  HTTP::Response response;
  publisher->handle(makeRequest("/edr/api/docs/swagger-ui.css"), response, "/edr", "/edr");
  BOOST_TEST(response.getStatus() == HTTP::Status::ok);
  BOOST_TEST(header(response, "Content-Type") == "text/css; charset=UTF-8");
  BOOST_TEST(response.getContent() == "content of swagger-ui.css");

  HTTP::Response missing;
  publisher->handle(makeRequest("/edr/api/docs/../../etc/passwd"), missing, "/edr", "/edr");
  BOOST_TEST(missing.getStatus() == HTTP::Status::not_found);

  // Conditional request
  HTTP::Response cached;
  publisher->handle(
      makeRequest("/edr/api/docs/swagger-ui.css", {{"If-None-Match", header(response, "ETag")}}),
      cached,
      "/edr",
      "/edr");
  BOOST_TEST(cached.getStatus() == HTTP::Status::not_modified);

  // Frontend ETag probe
  HTTP::Response probe;
  publisher->handle(makeRequest("/edr/api", {{"X-Request-ETag", "true"}}), probe, "/edr", "/edr");
  BOOST_TEST(probe.getStatus() == HTTP::Status::no_content);
  BOOST_TEST(!header(probe, "ETag").empty());
}

BOOST_AUTO_TEST_CASE(request_snippets)
{
  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());

  HTTP::Response response;
  publisher->handle(makeRequest("/edr/api/docs/smartmet-swagger-ui.js"), response, "/edr", "/edr");
  BOOST_TEST(response.getStatus() == HTTP::Status::ok);
  BOOST_TEST(header(response, "Content-Type") == "text/javascript; charset=UTF-8");

  HTTP::Response docs;
  publisher->handle(makeRequest("/edr/api/docs"), docs, "/edr", "/edr");
  const auto html = docs.getContent();
  BOOST_TEST(html.find("<script src=\"docs/smartmet-swagger-ui.js\"></script>") !=
             std::string::npos);
  BOOST_TEST(html.find("plugins: [SmartMetSwaggerUI.plugin]") != std::string::npos);
  BOOST_TEST(html.find("requestSnippetsEnabled: true") != std::string::npos);

  // Run the generated scripts, see openapi-snippets-test.js
  if (std::system("command -v node > /dev/null 2>&1") != 0)
  {
    BOOST_TEST_MESSAGE("node is not installed, the request snippets are not run");
    return;
  }
  const auto file = assets.path / "smartmet-swagger-ui.js";
  std::ofstream(file) << response.getContent();
  BOOST_TEST(std::system(("node openapi-snippets-test.js " + file.string()).c_str()) == 0);
}

BOOST_AUTO_TEST_CASE(browser_tests)
{
  // The page and the plugin as served, with the real Swagger UI files. SWAGGER_UI_DIR
  // overrides the location of the installed smartmet-swagger-ui files.
  if (std::system("command -v node > /dev/null 2>&1") != 0)
  {
    BOOST_TEST_MESSAGE("node is not installed, the browser tests are not run");
    return;
  }
  const char* env = std::getenv("SWAGGER_UI_DIR");
  const std::string swagger_ui = (env ? env : "/usr/share/smartmet/swagger-ui");

  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());
  HTTP::Response docs;
  publisher->handle(makeRequest("/edr/api/docs"), docs, "/edr", "/edr");
  HTTP::Response plugin;
  publisher->handle(makeRequest("/edr/api/docs/smartmet-swagger-ui.js"), plugin, "/edr", "/edr");
  std::ofstream(assets.path / "docs.html") << docs.getContent();
  std::ofstream(assets.path / "smartmet-swagger-ui.js") << plugin.getContent();

  // The documentation page of the API catalog
  libconfig::Config config;
  config.readString("api_catalog: { swagger_ui_dir = \"" + assets.path.string() + "\"; };");
  OpenAPI::Catalog catalog(OpenAPI::CatalogSettings::parse(config));
  HTTP::Response catalog_docs;
  catalog.handle(makeRequest("/api/docs"),
                 catalog_docs,
                 {{"/edr", "Viewer test", "", "", true}, {"/other", "Other API", "", "", true}});
  std::ofstream(assets.path / "catalog.html") << catalog_docs.getContent();

  const auto command = "node openapi-browser-test.js " + assets.path.string() + " " + swagger_ui;
  BOOST_TEST(std::system(command.c_str()) == 0);
}

BOOST_AUTO_TEST_CASE(explorer_tests)
{
  // The explorer page and files as served, with the explorer files of the source tree.
  // UPLOT_DIR overrides the location of the installed smartmet-uplot files.
  if (std::system("command -v node > /dev/null 2>&1") != 0)
  {
    BOOST_TEST_MESSAGE("node is not installed, the explorer tests are not run");
    return;
  }
  const char* env = std::getenv("UPLOT_DIR");
  const std::string uplot = (env ? env : "/usr/share/smartmet/uplot");
  if (!std::filesystem::exists(uplot + "/uPlot.iife.min.js"))
  {
    BOOST_TEST_MESSAGE("smartmet-uplot is not installed, the explorer tests are not run");
    return;
  }

  AssetDir assets;

  // A production palette and the parameter map, as qdless installs them
  std::filesystem::create_directories(assets.path / "palettes");
  std::ofstream(assets.path / "palettes" / "temperature.json")
      << R"({"name": "temperature", "isobands": [{"lo": -999, "hi": 12, "rgb": [0, 0, 255]},)"
      << R"( {"lo": 12, "hi": 14, "rgb": [0, 255, 0]}, {"lo": 14, "hi": 999, "rgb": [255, 0, 0]}]})";
  std::ofstream(assets.path / "palettes" / "broken.json") << "{";
  std::ofstream(assets.path / "qdless.conf")
      << R"({"_comment": "test", "palettes": {"Temperature": "temperature", "X": "missing"}})";

  libconfig::Config config;
  config.readString("api_catalog: { swagger_ui_dir = \"" + assets.path.string() +
                    "\"; explorer_dir = \"../explorer\"; uplot_dir = \"" + uplot +
                    "\"; palette_dir = \"" + (assets.path / "palettes").string() +
                    "\"; palette_config = \"" + (assets.path / "qdless.conf").string() + "\"; };");
  OpenAPI::Catalog catalog(OpenAPI::CatalogSettings::parse(config));
  const std::vector<OpenAPI::CatalogEntry> entries{
      {"/timeseries", "TimeSeries", "", "timeseries", true}};

  HTTP::Response page;
  BOOST_TEST(catalog.handle(makeRequest("/api/explorer"), page, entries));
  BOOST_TEST(page.getStatus() == HTTP::Status::ok);
  BOOST_TEST(header(page, "Content-Security-Policy").find("script-src 'self';") !=
             std::string::npos);
  HTTP::Response plugin;
  BOOST_TEST(catalog.handle(makeRequest("/api/explorer/smartmet-swagger-ui.js"), plugin, entries));
  BOOST_TEST(plugin.getStatus() == HTTP::Status::ok);
  HTTP::Response missing;
  BOOST_TEST(catalog.handle(makeRequest("/api/explorer/../OpenApi.cpp"), missing, entries));
  BOOST_TEST(missing.getStatus() == HTTP::Status::not_found);

  BOOST_TEST(page.getContent().find("data-palettes=") != std::string::npos);
  HTTP::Response palettes;
  BOOST_TEST(catalog.handle(makeRequest("/api/explorer/palettes.json"), palettes, entries));
  BOOST_TEST(palettes.getStatus() == HTTP::Status::ok);
  const auto paletteDoc = parse(palettes.getContent());
  BOOST_TEST(paletteDoc["palettes"]["temperature"].size() == 3U);
  BOOST_TEST(!paletteDoc["palettes"].isMember("broken"));
  BOOST_TEST(paletteDoc["parameters"]["temperature"].asString() == "temperature");
  BOOST_TEST(!paletteDoc["parameters"].isMember("x"));
  std::ofstream(assets.path / "palettes.json") << palettes.getContent();

  std::ofstream(assets.path / "explorer.html") << page.getContent();
  std::ofstream(assets.path / "smartmet-swagger-ui.js") << plugin.getContent();

  const auto command =
      "node explorer-browser-test.js " + assets.path.string() + " ../explorer " + uplot;
  BOOST_TEST(std::system(command.c_str()) == 0);
}

BOOST_AUTO_TEST_CASE(missing_swagger_ui)
{
  auto publisher = makePublisher("/nonexistent/swagger-ui");

  HTTP::Response docs;
  publisher->handle(makeRequest("/edr/api/docs"), docs, "/edr", "/edr");
  BOOST_TEST(docs.getStatus() == HTTP::Status::service_unavailable);
  BOOST_TEST(docs.getContent().find("/nonexistent") == std::string::npos);

  // The definition itself is still available
  HTTP::Response api;
  publisher->handle(makeRequest("/edr/api"), api, "/edr", "/edr");
  BOOST_TEST(api.getStatus() == HTTP::Status::ok);
}

BOOST_AUTO_TEST_CASE(format_parameter)
{
  AssetDir assets;
  auto publisher = makePublisher(assets.path.string());

  HTTP::Response bad;
  publisher->handle(makeRequest("/edr/api", {}, {{"f", "xml"}}), bad, "/edr", "/edr");
  BOOST_TEST(bad.getStatus() == HTTP::Status::bad_request);
  BOOST_TEST(header(bad, "Content-Type") == "application/problem+json");

  // f=json wins over a browser Accept header
  HTTP::Response json;
  publisher->handle(
      makeRequest("/edr/api", {{"Accept", "text/html"}}, {{"f", "json"}}), json, "/edr", "/edr");
  BOOST_TEST(header(json, "Content-Type") == OpenAPI::MEDIA_TYPE);
}

BOOST_AUTO_TEST_CASE(disabled_api)
{
  OpenAPI::Settings settings;
  settings.enabled = false;
  OpenAPI::Publisher publisher(settings);
  HTTP::Response response;
  BOOST_TEST(!publisher.handle(makeRequest("/edr/api"), response, "/edr", "/edr"));

  OpenAPI::Settings no_ui;
  no_ui.ui = false;
  OpenAPI::Publisher publisher2(no_ui);
  publisher2.publish(makeDocument(no_ui));
  HTTP::Response docs;
  BOOST_TEST(publisher2.handle(makeRequest("/edr/api/docs"), docs, "/edr", "/edr"));
  BOOST_TEST(docs.getStatus() == HTTP::Status::not_found);
  HTTP::Response html;
  publisher2.handle(makeRequest("/edr/api", {{"Accept", "text/html"}}), html, "/edr", "/edr");
  BOOST_TEST(header(html, "Content-Type") == OpenAPI::MEDIA_TYPE);
}

BOOST_AUTO_TEST_CASE(not_published_yet)
{
  OpenAPI::Settings settings;
  settings.ui = false;
  OpenAPI::Publisher publisher(settings);
  HTTP::Response response;
  publisher.handle(makeRequest("/edr/api"), response, "/edr", "/edr");
  BOOST_TEST(response.getStatus() == HTTP::Status::service_unavailable);
}

BOOST_AUTO_TEST_CASE(server_url)
{
  BOOST_TEST(OpenAPI::serverUrl(makeRequest("/edr/api", {{"Host", "example.org"}}), "/edr") ==
             "http://example.org/edr");
  BOOST_TEST(
      OpenAPI::serverUrl(
          makeRequest("/edr/api", {{"Host", "example.org:8080"}, {"X-Forwarded-Proto", "https"}}),
          "/edr/") == "https://example.org:8080/edr");
  BOOST_TEST(
      OpenAPI::serverUrl(
          makeRequest("/edr/api", {{"Host", "example.org"}, {"X-Forwarded-Proto", "javascript"}}),
          "/edr") == "http://example.org/edr");
  BOOST_TEST(OpenAPI::serverUrl(makeRequest("/edr/api", {{"Host", "evil\"<script>"}}), "/edr") ==
             "/edr");
  BOOST_TEST(OpenAPI::serverUrl(makeRequest("/edr/api"), "/edr") == "/edr");
  BOOST_TEST(OpenAPI::serverUrl(
                 makeRequest("/edr/api", {{"Host", "example.org"}, {"fmi-apikey", "abc-123"}}),
                 "/edr") == "http://example.org/fmi-apikey/abc-123/edr");
  BOOST_TEST(OpenAPI::serverUrl(makeRequest("/edr/api",
                                            {{"Host", "example.org"},
                                             {"fmi-apikey", "abc-123"},
                                             {"omit-fmi-apikey", "1"}}),
                                "/edr") == "http://example.org/edr");
  BOOST_TEST(OpenAPI::serverUrl(
                 makeRequest("/edr/api", {{"Host", "example.org"}, {"fmi-apikey", "a/../b"}}),
                 "/edr") == "http://example.org/edr");
}

BOOST_AUTO_TEST_CASE(prefers_html)
{
  BOOST_TEST(!OpenAPI::prefersHtml(makeRequest("/x")));
  BOOST_TEST(!OpenAPI::prefersHtml(makeRequest("/x", {{"Accept", "*/*"}})));
  BOOST_TEST(!OpenAPI::prefersHtml(makeRequest("/x", {{"Accept", "application/json"}})));
  BOOST_TEST(OpenAPI::prefersHtml(makeRequest("/x", {{"Accept", "text/html"}})));
  BOOST_TEST(OpenAPI::prefersHtml(makeRequest("/x", {{"Accept", "text/html, */*;q=0.8"}})));
  BOOST_TEST(
      !OpenAPI::prefersHtml(makeRequest("/x", {{"Accept", "text/html;q=0.5, application/json"}})));
  BOOST_TEST(OpenAPI::prefersHtml(makeRequest("/x", {}, {{"f", "html"}})));
  BOOST_TEST(!OpenAPI::prefersHtml(makeRequest("/x", {{"Accept", "text/html"}}, {{"f", "json"}})));
}

// ----------------------------------------------------------------------
// The API catalog
// ----------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(catalog_registry)
{
  AssetDir assets;
  BOOST_TEST(OpenAPI::localCatalogEntries().empty());
  {
    auto edr = makePublisher(assets.path.string());
    edr->addToCatalog("/edr/");  // the title comes from the definition
    auto tiles = makePublisher(assets.path.string());
    tiles->addToCatalog("/tiles", "Tiles", "Map tiles");

    OpenAPI::Settings disabled;
    disabled.enabled = false;
    OpenAPI::Publisher off(disabled);
    off.addToCatalog("/off", "Off");

    BOOST_CHECK_THROW(tiles->addToCatalog("relative"), std::exception);
    BOOST_CHECK_THROW(tiles->addToCatalog("/a/../b"), std::exception);
    BOOST_CHECK_THROW(tiles->addToCatalog("/a b"), std::exception);
    tiles->addToCatalog("/tiles", "Tiles", "Map tiles");

    auto entries = OpenAPI::localCatalogEntries();
    BOOST_REQUIRE(entries.size() == 2U);
    BOOST_TEST(entries[0].root == "/edr");
    BOOST_TEST(entries[0].title == "Test API");
    BOOST_TEST(entries[0].docs);
    BOOST_TEST(entries[1].root == "/tiles");
    BOOST_TEST(entries[1].description == "Map tiles");

    // A plugin reload creates the new publisher before the old one is destroyed
    auto reloaded = makePublisher(assets.path.string());
    reloaded->addToCatalog("/edr", "Reloaded");
    entries = OpenAPI::localCatalogEntries();
    BOOST_REQUIRE(entries.size() == 2U);
    BOOST_TEST(entries[0].title == "Reloaded");
    edr.reset();
    BOOST_TEST(OpenAPI::localCatalogEntries()[0].title == "Reloaded");

    // Without the Swagger UI files there is no documentation page
    auto nodocs = makePublisher("/nonexistent/swagger-ui");
    nodocs->addToCatalog("/coverages");
    BOOST_TEST(!OpenAPI::localCatalogEntries()[0].docs);
  }
  BOOST_TEST(OpenAPI::localCatalogEntries().empty());
}

BOOST_AUTO_TEST_CASE(catalog_linkset)
{
  std::vector<OpenAPI::CatalogEntry> entries{{"/edr", "EDR", "Environmental data", "edr", true},
                                             {"/wmts", "WMTS", "", "", false}};

  auto doc = OpenAPI::Catalog::linkset(
      makeRequest("/api", {{"Host", "smartmet.fmi.fi"}, {"X-Forwarded-Proto", "https"}}), entries);
  const auto& linkset = doc["linkset"];
  BOOST_REQUIRE(linkset.size() == 3U);
  BOOST_TEST(linkset[0]["anchor"].asString() == "https://smartmet.fmi.fi/.well-known/api-catalog");
  BOOST_TEST(linkset[0]["item"].size() == 2U);
  BOOST_TEST(linkset[0]["item"][0]["href"].asString() == "https://smartmet.fmi.fi/edr");
  BOOST_TEST(linkset[0]["item"][0]["title"].asString() == "EDR");
  BOOST_TEST(linkset[0]["item"][0]["description"].asString() == "Environmental data");
  BOOST_TEST(!linkset[0]["item"][1].isMember("description"));
  BOOST_TEST(linkset[0]["item"][0]["x-smartmet-kind"].asString() == "edr");
  BOOST_TEST(!linkset[0]["item"][1].isMember("x-smartmet-kind"));
  BOOST_TEST(linkset[1]["anchor"].asString() == "https://smartmet.fmi.fi/edr");
  BOOST_TEST(linkset[1]["service-desc"][0]["href"].asString() == "https://smartmet.fmi.fi/edr/api");
  BOOST_TEST(linkset[1]["service-desc"][0]["type"].asString() == OpenAPI::MEDIA_TYPE);
  BOOST_TEST(linkset[1]["service-doc"][0]["href"].asString() ==
             "https://smartmet.fmi.fi/edr/api/docs");
  BOOST_TEST(!linkset[2].isMember("service-doc"));

  // Without a Host the URLs are relative, which is what the frontend asks for
  auto relative = OpenAPI::Catalog::linkset(makeRequest("/api"), entries);
  BOOST_TEST(relative["linkset"][0]["item"][0]["href"].asString() == "/edr");

  // Parsing gives the entries back, also from absolute URLs and apikey paths
  BOOST_TEST((OpenAPI::parseCatalog(doc) == entries));
  BOOST_TEST((OpenAPI::parseCatalog(relative) == entries));
  auto keyed = OpenAPI::Catalog::linkset(
      makeRequest("/api", {{"Host", "smartmet.fmi.fi"}, {"fmi-apikey", "abc-123"}}), entries);
  BOOST_TEST((OpenAPI::parseCatalog(keyed) == entries));

  // Anything but plain paths is skipped
  Json::Value bad;
  for (const char* href : {"javascript:alert(1)",
                           "http://host",
                           "/a/../b",
                           "/a?b=1",
                           "/a\"><script>",
                           "//other.host/x",
                           "/fmi-apikey/x"})
  {
    Json::Value item;
    item["href"] = href;
    bad["linkset"][0]["item"].append(item);
  }
  Json::Value ok;
  ok["href"] = "/ok";
  bad["linkset"][0]["item"].append(ok);
  bad["linkset"][0]["item"].append(ok);  // duplicates are merged
  auto parsed = OpenAPI::parseCatalog(bad);
  BOOST_REQUIRE(parsed.size() == 1U);
  BOOST_TEST(parsed[0].root == "/ok");
  BOOST_TEST(parsed[0].title == "/ok");
  BOOST_TEST(!parsed[0].docs);
  BOOST_TEST(OpenAPI::parseCatalog(parse("[1,2]")).empty());
}

BOOST_AUTO_TEST_CASE(catalog_requests)
{
  AssetDir assets;
  libconfig::Config config;
  config.readString(
      "api_catalog: { title = \"FMI <APIs>\"; description = \"All APIs\"; swagger_ui_dir = \"" +
      assets.path.string() + "\"; };");
  OpenAPI::Catalog catalog(OpenAPI::CatalogSettings::parse(config));
  BOOST_TEST(catalog.settings().title == "FMI <APIs>");

  std::vector<OpenAPI::CatalogEntry> entries{{"/edr", "EDR <script>", "Data", "", true},
                                             {"/timeseries", "TimeSeries", "", "", true}};

  const auto get = [&](const std::string& resource,
                       const HTTP::HeaderMap& headers = {},
                       const HTTP::ParamMap& params = {})
  {
    HTTP::Response response;
    BOOST_TEST(catalog.handle(makeRequest(resource, headers, params), response, entries));
    return response;
  };

  // The well-known catalog
  auto wk = get("/.well-known/api-catalog", {{"Host", "example.org"}});
  BOOST_TEST(wk.getStatus() == HTTP::Status::ok);
  BOOST_TEST(header(wk, "Content-Type") ==
             "application/linkset+json; profile=\"https://www.rfc-editor.org/info/rfc9727\"");
  BOOST_TEST(header(wk, "Link") ==
             "<http://example.org/.well-known/api-catalog>; rel=\"api-catalog\"");
  BOOST_TEST(header(wk, "Access-Control-Allow-Origin") == "*");
  BOOST_TEST(parse(wk.getContent())["linkset"][0]["item"].size() == 2U);

  // /api is the same, or an HTML page
  BOOST_TEST(get("/api").getContent() == get("/.well-known/api-catalog").getContent());
  auto page = get("/api", {{"Accept", "text/html,application/xhtml+xml,*/*;q=0.8"}});
  BOOST_TEST(header(page, "Content-Type") == "text/html; charset=UTF-8");
  const auto html = page.getContent();
  BOOST_TEST(html.find("<title>FMI &lt;APIs&gt;</title>") != std::string::npos);
  BOOST_TEST(html.find("EDR &lt;script&gt;") != std::string::npos);
  BOOST_TEST(html.find("<script") == std::string::npos);
  BOOST_TEST(html.find("href=\"edr/api/docs\"") != std::string::npos);
  BOOST_TEST(html.find("href=\"timeseries/api\"") != std::string::npos);
  BOOST_TEST(html.find("href=\"api/docs\"") != std::string::npos);
  BOOST_TEST(html.find("href=\".well-known/api-catalog\"") != std::string::npos);
  BOOST_TEST(header(page, "Content-Security-Policy").find("default-src 'none'") !=
             std::string::npos);
  BOOST_TEST(get("/api", {}, {{"f", "html"}}).getContent() == html);
  BOOST_TEST(get("/api", {{"Accept", "text/html"}}, {{"f", "json"}}).getContent() ==
             get("/api").getContent());
  BOOST_TEST(get("/api", {}, {{"f", "xml"}}).getStatus() == HTTP::Status::bad_request);

  // The documentation page lists every definition in the selector
  auto docs = get("/api/docs");
  BOOST_TEST(docs.getStatus() == HTTP::Status::ok);
  const auto docs_html = docs.getContent();
  BOOST_TEST(docs_html.find("SwaggerUIStandalonePreset") != std::string::npos);
  BOOST_TEST(docs_html.find("\"url\":\"../edr/api\"") != std::string::npos);
  BOOST_TEST(docs_html.find("\"name\":\"EDR \\u003cscript>") != std::string::npos);
  BOOST_TEST(docs_html.find("<script>\"") == std::string::npos);
  BOOST_TEST(docs_html.find("href=\"docs/swagger-ui.css\"") != std::string::npos);
  BOOST_TEST(header(docs, "Content-Security-Policy").find("script-src 'self'") !=
             std::string::npos);

  // The Swagger UI files
  auto css = get("/api/docs/swagger-ui.css");
  BOOST_TEST(css.getStatus() == HTTP::Status::ok);
  BOOST_TEST(css.getContent() == "content of swagger-ui.css");
  BOOST_TEST(get("/api/docs/smartmet-swagger-ui.js").getStatus() == HTTP::Status::ok);
  BOOST_TEST(get("/api/docs/../../etc/passwd").getStatus() == HTTP::Status::not_found);

  // ETags
  auto probe = get("/api", {{"X-Request-ETag", "true"}});
  BOOST_TEST(probe.getStatus() == HTTP::Status::no_content);
  BOOST_TEST(get("/api", {{"If-None-Match", header(probe, "ETag")}}).getStatus() ==
             HTTP::Status::not_modified);

  // Not catalog resources
  HTTP::Response other;
  BOOST_TEST(!catalog.handle(makeRequest("/edr/api"), other, entries));
  BOOST_TEST(!catalog.handle(makeRequest("/api/docsx"), other, entries));

  // No documentation page without the Swagger UI or with ui = false
  libconfig::Config noui;
  noui.readString("api_catalog: { ui = false; };");
  OpenAPI::Catalog plain(OpenAPI::CatalogSettings::parse(noui));
  HTTP::Response r1;
  BOOST_TEST(plain.handle(makeRequest("/api/docs"), r1, entries));
  BOOST_TEST(r1.getStatus() == HTTP::Status::not_found);
  HTTP::Response r2;
  BOOST_TEST(plain.handle(makeRequest("/api"), r2, entries));
  BOOST_TEST(r2.getStatus() == HTTP::Status::ok);

  // Disabled
  libconfig::Config off;
  off.readString("api_catalog: { enabled = false; };");
  OpenAPI::Catalog disabled(OpenAPI::CatalogSettings::parse(off));
  HTTP::Response r3;
  BOOST_TEST(!disabled.handle(makeRequest("/api"), r3, entries));
}
