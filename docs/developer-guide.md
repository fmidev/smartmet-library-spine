# spine developer guide

This guide is for developers who change `smartmet-library-spine`, or who write engines and
plugins on top of it. It explains how the server core is put together: how engines and
plugins are loaded and initialised, how requests are routed, how admin requests work, and
the building blocks spine offers to plugins. It ends with the rules for keeping binary
compatibility and a list of pitfalls.

[FEATURES.md](../FEATURES.md) lists spine's features. The HTTP server itself (sockets,
thread pools, compression, keep-alive) is in `smartmet-server`
([brainstorm/server](https://github.com/fmidev/smartmet-server)); spine is the part that
the server, the engines and the plugins all link.

## Contents

1. [What spine is](#1-what-spine-is)
2. [Building and testing](#2-building-and-testing)
3. [Source overview](#3-source-overview)
4. [Server startup: Options and the Reactor](#4-server-startup-options-and-the-reactor)
5. [Writing an engine](#5-writing-an-engine)
6. [Writing a plugin](#6-writing-a-plugin)
7. [Request routing](#7-request-routing)
8. [Admin and info requests](#8-admin-and-info-requests)
9. [HTTP requests and responses](#9-http-requests-and-responses)
10. [Output: Table, Value and the formatters](#10-output-table-value-and-the-formatters)
11. [Other building blocks](#11-other-building-blocks)
12. [Shutdown](#12-shutdown)
13. [Binary compatibility](#13-binary-compatibility)
14. [Known pitfalls](#14-known-pitfalls)

---

## 1. What spine is

`libsmartmet-spine.so` (namespace `SmartMet::Spine`) provides:

* the **Reactor**: it reads the server configuration, `dlopen`s the engines and plugins,
  initialises them in parallel, owns the URI → handler map, and coordinates shutdown;
* the **plugin and engine base classes** (`SmartMetPlugin`, `SmartMetEngine`) and the
  loading conventions (`create` / `destroy`, `engine_name` / `engine_class_creator`);
* **HTTP types** (`HTTP::Request`, `HTTP::Response`, `HTTP::ContentStreamer`), parsers,
  status codes, and basic authentication;
* **admin requests** (`/admin?what=…`, `/info?what=…`) with access levels;
* **output**: `Table`, `Value` and the formatters (ASCII, CSV, JSON, XML, WXML, HTML,
  PHP, serial, debug, image);
* shared utilities: configuration helpers (`ConfigBase`, `ConfigTools`), parameter
  handling (`Parameter`, `ParameterTranslations`), caches (`SmartMetCache`, `FileCache`,
  `JsonCache`), `CRSRegistry`, `IPFilter`, `HostInfo`, `TcpMultiQuery`, access logging,
  active-request tracking and OpenTelemetry hooks;
* **`smartmet-plugin-test`** and **`cfgvalidate`** (built from `app/`), the tools that
  every plugin's test suite uses.

## 2. Building and testing

```bash
make                     # libsmartmet-spine.so + app/ (smartmet-plugin-test, cfgvalidate)
make test                # unit tests + reactor tests
make -C test FooTest     # build one unit test; run ./test/FooTest
make -C test TSAN=yes test
make -C test ASAN=yes test
make format              # clang-format (unlike the grid repositories, spine is formatted)
```

* **Unit tests** are Boost.Test programs, `test/*Test.cpp`, one executable each.
* **Reactor tests** (`test/reactor_tests/`) build a tiny test engine and test plugin,
  load them into a real Reactor with `reactor.conf`, and run requests from `input/`
  (`*.get`, with optional `*.options`) against the expected responses in `output/`.
  They cover loading, admin requests in every format, and shutdown.
* **`smartmet-plugin-test`** (`app/SmartmetPluginTest.cpp`, `app/PluginTest.h`) is the
  same harness in general form: `smartmet-plugin-test -c reactor.conf [-i input]
  [-e output] [-f failures]`. It starts a Reactor with the given configuration, waits
  until initialisation is done, runs every request file in the input directory, and
  compares the responses with the expected outputs. It writes mismatches to the failures
  directory. A `.testignore` file lists tests to skip. Every plugin's `make test` uses it.
  Only the response body is compared, except for OPTIONS requests. To check the HTTP status
  and selected response headers too, add a request header such as
  `X-Test-Headers: Content-Type,Link`: the result then starts with `Status: <code>` and one
  `Name: value` line per listed header (`(none)` if missing), followed by an empty line and
  the body. An empty header value checks the status only.
  Responses that depend on the environment (versions, paths, build-ids) can be validated
  with an expected output `<test>.regex` instead: the whole result must match it. The
  pattern uses Perl syntax in free-spacing mode, so it may span several lines and contain
  `#` comments, literal spaces must be escaped, and `(?(DEFINE)...)` with `(?&name)` keeps
  repeated sub-patterns readable. See `test/reactor_tests/output/packages_json.get.regex`.

## 3. Source overview

| Area | Files |
|------|-------|
| Core | `Reactor`, `ContentHandlerMap`, `HandlerView`, `DynamicPlugin`, `SmartMetEngine`, `SmartMetPlugin`, `SmartMet.h` (API version), `Options`, `Names`, `Thread` |
| HTTP | `HTTP`, `HTTPParsers`, `HTTPAuthentication`, `IPFilter`, `FmiApiKey`, `ActiveRequests`, `ActiveBackends`, `LoggedRequest`, `AccessLogger`, `LogRange` |
| Output | `Table`, `TableFormatter` and its subclasses, `TableFormatterFactory`, `TableFormatterOptions`, `TableVisitor`, `Value`, `None` |
| Configuration | `ConfigBase`, `ConfigTools`, `Convenience` |
| Weather domain | `Parameter`, `Parameters`, `ParameterTranslations`, `Location`, `Station`, `LonLat`, `QCConverter`, `MultiLanguageString(Array)`, `Translations`, `StringTranslations` |
| Caches | `SmartMetCache`, `FileCache`, `JsonCache` |
| Other | `CRSRegistry`, `HostInfo`, `TcpMultiQuery`, `Json`, `Backtrace`, `MallocStats`, `LoadedPackages`, `Exceptions`, `OTel*` |

## 4. Server startup: Options and the Reactor

`smartmetd` parses its command line and the main configuration file into `Options`: the
port and TLS settings, the three **thread pools** (`adminpool`, `slowpool`, `fastpool`:
`minsize`, `maxrequeuesize`), the **throttle** (`start_limit`, `restart_limit`, `limit`,
`increase_rate`, the active-request limits behind `isLoadHigh()`), compression, logging,
`lazylinking`, client host name resolution, `maxrequestsize`, the
`staleWhileRevalidate` / `staleIfError` cache headers, and OpenTelemetry. It then
creates the `Reactor` and calls `init()`.

`Reactor::init()`:

1. **Engines.** For each group under `engines` (skipping those with `disabled = true`),
   `loadEngine()`:
   * checks that the engine's configuration file (`engines.<name>.configfile`) is readable;
   * `dlopen`s `<moduledir>/engines/<name>.so` (or `libfile`) with
     **`RTLD_NOW | RTLD_GLOBAL`**. Global, so that plugins loaded later can resolve the
     engine's symbols. A load failure reports the demangled missing symbol;
   * resolves `engine_name` and `engine_class_creator`, calls the creator (this runs the
     engine's **constructor**, on the main thread), and stores the singleton;
   * queues the engine's `init()` as an asynchronous task (`initializeEngine()` →
     `SmartMetEngine::construct()`).
2. **Plugins.** For each group under `plugins`, `loadPlugin()` reads the plugin's
   `configfile` and `ip_filters`, creates a `DynamicPlugin` (which `dlopen`s the plugin
   with `RTLD_LAZY` when `lazylinking` is on, otherwise `RTLD_NOW`, resolves `create`
   and `destroy`, constructs the plugin, and checks the API version), and queues the
   plugin's `init()` as another asynchronous task.
3. It **waits for all tasks**. If any engine or plugin initialisation throws, the server
   start fails.
4. It sets the default logging, starts the OpenTelemetry metrics exporter if configured,
   and marks initialisation done. `/admin?what=waitforready` (`waitForReady()`) returns
   `ready in …` from then on; `smartmet-plugin-test` uses it to wait for the start.

So engines and plugins **initialise concurrently**. A plugin that needs an engine calls
`getEngine<T>(name)`, which blocks until that engine's `init()` has finished
(`SmartMetEngine::wait()`). An engine that uses another engine does the same.

## 5. Writing an engine

```cpp
class Engine : public SmartMet::Spine::SmartMetEngine
{
 public:
  explicit Engine(const std::string& configfile);  // read configuration here; keep it fast
 protected:
  void init() override;       // heavy initialisation; runs in its own task
  void shutdown() override;   // stop threads, release resources
};

extern "C" void* engine_class_creator(const char* configfile, void* /* user_data */)
{
  return new Engine(configfile);
}
extern "C" const char* engine_name()
{
  return "myengine";   // the name plugins pass to getEngine<>()
}
```

Rules:

* The constructor runs sequentially for all engines, on the main thread, so keep it
  quick: read and check the configuration, then do the real work in `init()`.
* `init()` runs in parallel with the other engines' and plugins' `init()`. Use
  `itsReactor->getEngine<Other>("other")` for dependencies, and never take a lock that
  another engine's `init()` might hold while it waits for you. That deadlocks startup.
* Check `Reactor::isShuttingDown()` in long loops in `init()`, so that a failed start
  (or Ctrl-C) does not wait for your initialisation to finish.
* Override `getCacheStats()` to publish your caches in `/admin?what=cachestats` and the
  OpenTelemetry metrics.
* `shutdown()` must stop every thread the engine started. The Reactor destroys engines
  after all plugins have shut down ([§12](#12-shutdown)).
* Register admin requests with `itsReactor->addAdmin…RequestHandler(this, …)` in
  `init()` ([§8](#8-admin-and-info-requests)).

## 6. Writing a plugin

```cpp
class Plugin : public SmartMetPlugin
{
 public:
  Plugin(SmartMet::Spine::Reactor* theReactor, const char* theConfig);
  const std::string& getPluginName() const override;
  int getRequiredAPIVersion() const override { return SMARTMET_API_VERSION; }
  bool queryIsFast(const SmartMet::Spine::HTTP::Request&) const override;   // optional
  bool isAdminQuery(const SmartMet::Spine::HTTP::Request&) const override;  // optional
 protected:
  void init() override;
  void shutdown() override;
  void requestHandler(SmartMet::Spine::Reactor&, const SmartMet::Spine::HTTP::Request&,
                      SmartMet::Spine::HTTP::Response&) override;
};

extern "C" SmartMetPlugin* create(SmartMet::Spine::Reactor* them, const char* config)
{
  return new Plugin(them, config);
}
extern "C" void destroy(SmartMetPlugin* us) { delete us; }
```

* **Register the URIs** in the constructor or in `init()`:
  `theReactor->addContentHandler(this, "/myplugin", boost::bind(&Plugin::callRequestHandler,
  this, _1, _2, _3))`. Optional arguments name the POST content types the handler accepts
  and whether it handles every URI with this **prefix**. `addPrivateContentHandler()`
  registers a URI that is **not advertised** ([§7](#7-request-routing)).
* **Go through `callRequestHandler()`**, not `requestHandler()` directly. It counts
  requests and responses and rejects requests while the plugin is still initialising or
  shutting down.
* **Get engines in `init()`**, not in the constructor. The constructor runs before any
  engine is initialised.
* **`queryIsFast()`** decides the thread pool: fast queries go to `fastpool`, the rest to
  `slowpool` (if it has threads). **`isAdminQuery()`** sends the request to
  `adminpool`, which is not subject to the high-load check. Neither restricts who may
  call the plugin.
* `getRequiredAPIVersion()` must return `SMARTMET_API_VERSION`. `DynamicPlugin`
  refuses to load a plugin built against a different API version.

## 7. Request routing

`ContentHandlerMap::getHandlerView(request)` maps the request's resource to a
`HandlerView`. If the resource starts with a URI registered as a **prefix** handler
(followed by `/` or the end), the first such prefix in sorted order is used; otherwise the
resource must match a registered URI exactly. Anything else goes to the "no match"
handler (the frontend plugin installs one) or gets a 404. A
The `HandlerView` wraps the plugin's handler with the plugin's IP filter (a non-matching
client is refused), the access log for the URI, and last-request bookkeeping.

The server (`AsyncConnection`) then chooses a pool:

* a frontend's catch-all ("no match") handler always uses the fast pool;
* `isAdminQuery()` and a non-empty admin pool → admin pool (and skips the `isLoadHigh()`
  check, which otherwise answers with the `high_load` status);
* otherwise `queryIsFast()` → fast pool, else slow pool;
* a full queue answers `503 Service Unavailable`.

**Public and private URIs.** `getURIMap()` lists only **public** handlers. The sputnik
engine broadcasts that list to the frontends, and frontends only forward the URIs they
have heard about. A **private** handler is therefore not routed **through a
frontend**. A private handler whose plugin has no `plugins.<name>.ip_filters` accepts
only localhost and the private networks 10.0.0.0/8, 172.16.0.0/12 and 192.168.0.0/16;
`ip_filters` replaces this default. The filters are checked by the handler view, and
the section name is matched to the plugin name ignoring case, dashes and underscores
(`grid-gui` matches `GridGui`).

## 8. Admin and info requests

Engines and plugins register named admin requests:

```cpp
reactor->addAdminTableRequestHandler(this, "gridproducers", AdminRequestAccess::Public,
    std::bind(&Engine::requestGridProducerInfo, this, std::placeholders::_2),
    "Grid producers");
```

There are four handler types: **bool** (success or failure), **table** (the Reactor
formats the returned `Table` in the format the request asks for: `format=json|ascii|
debug|serial|…`), **string** and **custom** (the handler writes the response itself).
Requests are made as `<admin uri>?what=<name>` (default `/admin`). The access levels are:

| Access | Meaning |
|--------|---------|
| `Public` | No authentication; also available at `/info?what=<name>`, and listed by `what=list`. |
| `Private` | No authentication; not available through `/info`. |
| `RequiresAuthentication` | HTTP Basic authentication with `admin.user` / `admin.password` from the server configuration. Without them the request is still registered, but it is refused with 403 unless the caller (such as the admin or frontend plugin) authenticates it itself. |

The admin URI is set by `admin.uri`, and the allowed clients by `admin.ip_filters`.
Always configure `admin.ip_filters` together with `admin.uri`.

The Reactor registers its own requests: `list`, `lastrequests`, `activerequests`,
`cachestats`, `servicestats`, `engineinfo`, `plugininfo`, `packages`, `waitforready`,
`mallocstats`, the logging switches, and others; the server's `docs/Admin-Requests.md` describes them from
the user side. Handlers registered by a plugin or engine are removed automatically when
it shuts down (`removeContentHandlers()`).

## 9. HTTP requests and responses

`HTTP::Request`:

* `getResource()`, `getMethod()`, `getClientIP()`, `getHeader(name)` (case-insensitive),
  `getContent()`;
* `getParameter(name)` → `std::optional<std::string>`, `getParameterList(name)` for
  repeated parameters, `getParameterMap()`, and `addParameter()` /
  `removeParameter()` / `setParameter()` for plugins that rewrite requests.

Parameter parsing is done by `HTTPParsers`: query strings, and for POST, form data or the
content types the handler declared. Use the `Convenience.h` helpers
(`optional_int(req.getParameter("x"), 10)`, `optional_bool`, `optional_time`,
`required_string`, …) to read parameters with defaults and error messages.

`HTTP::Response`:

* `setStatus(HTTP::Status::ok)` (the `Status` enum; `high_load = 1234` is internal);
* `setHeader(name, value)`: set `Content-Type`, `Cache-Control`, `ETag`, `Expires` and
  so on yourself;
* `setContent(...)`: a string, a shared buffer, or an **`HTTP::ContentStreamer`**. A
  streamer's `getChunk()` is called repeatedly and the response is sent chunked, so large
  outputs (the download plugin's GRIB streams) do not have to be built in memory. Set
  the streamer status to report success or failure after the headers have been sent.

Conditional requests: set an `ETag` and answer `If-None-Match` with 304 yourself (see how
grid-gui or wms do it). The frontend caches responses by ETag.

## 10. Output: Table, Value and the formatters

Most plugins produce tables of values:

* `Spine::Table` is a sparse two-dimensional table of strings, with optional column names
  and title (`set(column, row, value)`, `setNames()`, `setTitle()`).
* `TableFormatterFactory::create(name)` returns a formatter for `ascii`, `csv`, `json`,
  `xml`, `wxml`, `html`, `php`, `serial`, `debug` or `image`.
  `formatter->format(table, names, request, options)` returns the text, and
  `mimetype()` the content type. `TableFormatterOptions` holds the shared settings
  (missing-value text, …).
* `Spine::Value` is a typed request-parameter value: a variant of empty, bool, signed
  and unsigned integers, double, string, time, point and bounding box, with parsing
  (including relative times such as `3 hours ago rounded 5 min`) and checked conversions.
  The WFS plugin's stored queries use it. The values of time series tables are the
  separate `TimeSeries::Value` type from the timeseries library.

## 11. Other building blocks

* **Configuration.** `ConfigBase` wraps a libconfig file with typed getters
  (`get_mandatory_config_param<T>`, `get_optional_config_param<T>`, path handling
  relative to the file). `ConfigTools` implements the `@include` / environment variable
  handling and host-specific settings used by the Reactor (`lookupHostSetting`,
  `lookupPathSetting`, …). `cfgvalidate` checks a file from the command line.
* **Caches.** `SmartMetCache` is a two-level cache: a memory LRU sized in bytes
  (`Fmi::Cache::Cache` with one shard, for exact byte accounting), plus an optional file
  cache that entries evicted from memory are written to by a background thread.
  `FileCache` is the file level on its own, and `JsonCache` caches parsed JSON files.
  Report cache statistics through `getCacheStats()`.
* **`CRSRegistry`** maps CRS names and EPSG codes to GDAL spatial references with
  per-CRS attributes (axis order, bbox), for the OGC plugins.
* **`IPFilter`** is the IP allow-list used for plugin `ip_filters` and the admin URI.
* **`HostInfo`** resolves client host names (cached, in background threads, controlled
  by the `clientHostName*` options). **`TcpMultiQuery`** sends several TCP queries in
  parallel; the frontend uses it to query its backends' state.
* **Active requests and access logs.** `ActiveRequests` tracks running requests
  (`what=activerequests`); a terminate handler prints them if the process dies.
  `AccessLogger` writes per-URI access logs when logging is on.
* **OpenAPI** (`OpenApi.h`): plugins describe their APIs with generated OpenAPI 3.1
  definitions. Build a `Spine::OpenAPI::Document` from code, configuration and metadata
  (`addCommonComponents()` and `addCoreOperations()` add the OGC API - Common parts),
  hand it to a `Publisher` whenever the metadata changes, and let `Publisher::handle()`
  answer `<root>/api` (the JSON definition, or the documentation page for `f=html`),
  `<root>/api/docs` (Swagger UI) and the Swagger UI files below it. The files come from a
  local directory (package `smartmet-swagger-ui`), never from a CDN. The definition is
  serialized once per `publish()`; requests only insert the server URL from
  `OpenAPI::serverUrl()`. Settings are read from the plugin's `openapi` group with
  `Settings::parse()`. The documentation page shows the URL of the request while the
  parameters are entered, and the request as bash,
  Python, R and JavaScript snippets, and as scripts which repeat the request for
  consecutive days, weeks, months or years when it has a time window (`datetime=start/end`
  or `starttime` and `endtime`). JSON and XML responses are shown as a collapsible tree.
  A parameter with the `x-smartmet-locations` extension (`path`, `parameter`, `language`,
  `query`) gets a button which shows the locations of its value on a map, with the details
  of each location on mouseover; the EDR plugin adds it to the TimeSeries `keyword` and
  `inkeyword` parameters. The map background is the Web Mercator tile layer of the
  `map_tiles` setting (normally an OGC API - Tiles layer of the same server; an absolute
  URL's origin is added to the page's `img-src` policy), or a longitude/latitude grid.
  Only the EDR plugin's TimeSeries pages have such parameters, so the `map_*` settings
  have no effect in the other plugins.
  The plugin is in `OpenApiPlugin.cpp`; `test/openapi-snippets-test.js` runs the generated
  scripts against a local server and `test/openapi-browser-test.js` checks the tree, the
  request preview and the map in a headless browser.
* **API catalog** (`OpenApi.h`, RFC 9727): `Publisher::addToCatalog(root, title,
  description)` lists an API in the catalog of the server for as long as the publisher
  exists (a reload's new publisher replaces the old one for the same root). After the
  plugins are initialized, the Reactor serves the catalog of these APIs if there are any
  and no plugin serves it already: `/.well-known/api-catalog` and `/api` (a linkset,
  `application/linkset+json` with the RFC 9727 profile; `/api` gives an HTML page for
  `f=html` or browsers), and `/api/docs` (Swagger UI with every definition in the top bar
  selector). The handlers are public, so sputnik tells the frontends which backends have a
  catalog; the frontend serves the merged catalog of the cluster with `OpenAPI::Catalog`
  and `parseCatalog()`. URLs are absolute when the request has a valid `Host`, relative
  otherwise (the frontend asks the backends without one). Settings are read from the
  `api_catalog` group of the server configuration (`enabled`, `ui`, `swagger_ui_dir`,
  `title`, `description`, `explorer`, `explorer_dir`, `uplot_dir`). An API added with a
  `kind` (`edr`, `timeseries`, `coverages`, `tiles`, `wmts`) carries it as
  `x-smartmet-kind` in the catalog.
* **Data explorer** (`explorer/`, served by `OpenAPI::Catalog` at `/api/explorer`): a page
  that builds queries from the metadata of the APIs in the catalog instead of from their
  OpenAPI definitions. For TimeSeries (kind `timeseries`) it loads
  `<root>/api/producers` and lets the parameters and the producers narrow each other with
  autocomplete, sets the times from the producer's data, and draws the results with uPlot:
  a chart per parameter with a line per location, or for more than 20 locations the
  median and the 10/25/75/90 % percentiles or a swarm of thin lines coloured from south to
  north with the median, and a table and the JSON tree. The map shows the area of the
  producer, the locations of a place, keyword, station or area name and of the results,
  and lets the user click a point, drag a radius or drag a box; a selected area narrows the
  producers to those covering it. Area and radius results, whose values TimeSeries returns
  as lists, are split into a row per point.
  For areas, radii, boxes and (grouped with `grouplocations=1`) keywords and station lists,
  the server can compute the median and the percentiles instead (`nanpercentile_a[p]`,
  `nanmedian_a`), which scales to large areas; "Every point" fetches all the values for the
  swarm. For EDR (kind `edr`) the explorer reads `<root>/collections`: the parameters and
  the collections narrow each other, the query types are those of the collection
  (position, radius, area, cube and locations, with the location list of the collection on
  the map), the times and the levels come from the extents, and CoverageJSON and GeoJSON
  responses are turned into rows for the same charts and tables, with a series per
  location and level. For Coverages (kind `coverages`) the parameters come from the schemas
  of the collections, the card shows the native projection, grid and resolution, the map
  shows the native grid, and as the box, the output projection, the grid size, the times,
  the parameters or the levels change, `<root>/collections/{id}/outline` gives the area the
  download would cover (drawn on the map) and its size; the result is a download link and
  the code of the request. Results with values at several locations also get a Map tab:
  the values of a parameter at a time as coloured cells (grids, sized by the median
  distance to the nearest point) or dots (stations), with a time slider, a play button and
  an editable colour scale (palette, minimum and maximum, continuous or in classes). The
  isoband colours of the production products, as converted by qdless from `wms-conf`
  (`palette_dir`, by default `/usr/share/smartmet/qdless/palettes`, and the parameter map of
  `palette_config`, `/usr/share/smartmet/qdless/qdless.conf`), are served as
  `palettes.json` and used by default for the parameters they are mapped to: the values
  are coloured band by band and values outside the bands are left out, as in qdless. The state is kept in the URL fragment, so a query can be shared as a
  link. Without a tile layer (`map_tiles`) the map draws the land, the lakes and
  the country borders of `basemap.json`, generated from the public domain Natural Earth 1:50m
  data by `explorer/make-basemap.py` (see the script for the source), so that areas can be
  chosen without any online map service. `explorer.js`, `explorer.css` and `basemap.json`
  are installed to `/usr/share/smartmet/explorer`,
  uPlot comes from the `smartmet-uplot` package (`/usr/share/smartmet/uplot`), and the
  snippets, the tree and the map are those of `OpenApiPlugin.cpp`. The page loads no inline
  scripts. `test/explorer-browser-test.js` checks it in a headless browser
  (`UPLOT_DIR` overrides the uPlot location).
- The explorer has no regional defaults of its own: without configuration the maps open on
  the whole world and the location fields show neutral examples. `api_catalog.explorer_defaults`
  sets them for the region and the users of a server: `center` (longitude, latitude) and `zoom`
  of the first map view (point, box and path examples are made from the center), `examples`
  of the `place`, `fmisid`, `wmo`, `keyword` and `area` fields, extra Coverages output
  `projections` (`"EPSG:3067 ETRS-TM35FIN"`; the native grid, EPSG:4326 and EPSG:3857 are
  always offered), the `clock` (`"24"` or `"12"`) and `units` (`temperature` C|F|K, `speed`
  ms|kt|kmh|mph, `pressure` hPa|Pa|inHg, `height` m|ft). The settings are validated when the
  configuration is read and passed to the page as the JSON attribute `data-defaults`.
- The settings menu of the page (clock, units, language, licences) overrides the defaults per
  browser. Without a choice or a server default the units and the clock follow the language
  of the browser, except that en-US is only taken as the United States when the time zone of
  the browser is in the Americas or the Pacific (it is the language of many browsers
  elsewhere). Temperatures in K (raw GRIB data) and pressures in Pa are recognised by their
  values and shown in the units chosen, also on the Coverages maps. Times are entered as text
  in the clock chosen (`2026-10-17 13:00` or `2026-10-17 1:00 PM`, UTC) with a button for the
  date picker of the browser, since `datetime-local` inputs always follow the language of the
  browser.
- The texts of the page are in English in `explorer.js`, wrapped in `L('...')` or, with values,
  tagged as ``L`${n} rows` `` (translation key `{0} rows`). The translations are
  `explorer/explorer-<code>.js` (Finnish `fi` and Swedish `sv`), one object of English text to
  translation each; every such file in `explorer_dir` is loaded before `explorer.js`. The
  language is the choice of the settings menu (a new language reloads the page), else
  `explorer_defaults.language`, else the first language of the browser with a translation, else
  English. Keys, tab names and the state in the address stay English. The browser test checks
  that every `L('...')` text is in both translations and that the views shown in Finnish and
  Swedish ask for no text without one; a new text needs its translations in both files.
* **OpenTelemetry** (`OTelOptions`, `OTelLogger`, `OTelMetricsExporter`) is optional.
  Tracing is compiled in with `SMARTMET_SPINE_OPENTELEMETRY`
  (see [build-opentelemetry.md](build-opentelemetry.md)), and cache statistics are
  exported as metrics.

## 12. Shutdown

`Reactor::requestShutdown()` (from a signal) sets the shutdown flag.
`Reactor::isShuttingDown()` is what all long-running code should check. The shutdown
(`shutdown_impl()`) then:

1. **shuts down the plugins** (`shutdownPlugin()` → your `shutdown()`), and removes their
   handlers;
2. **shuts down the engines** (`shutdownEngine()` → your `shutdown()`);
3. **destroys the engines** once no one holds a reference to them.

A watch thread enforces a timeout (60 s by default): if the shutdown takes too long, the
`onShutdownTimedOut` callback runs, and by default the process `abort()`s. Keep
`shutdown()` short, make background threads wake up promptly, and do not start new work
after `isShuttingDown()` is true.

## 13. Binary compatibility

The server, the engines and the plugins are separate shared objects built from separate
packages. The rules that keep them working together:

* **API version.** `SMARTMET_API_VERSION` (in `SmartMet.h`) is compiled into the server
  and into each plugin, and they must match. Change it only for an incompatible change of
  the plugin interface, and then rebuild every plugin.
* **Engine and plugin interfaces are C++ classes**, not a stable ABI. Plugins call engine
  methods directly; the symbols are resolved when the plugin is loaded (and lazily, with
  `lazylinking`). Adding a non-virtual method is safe. Changing a signature makes old
  plugins fail to load (an unresolved symbol, reported demangled). **Adding, removing or
  reordering virtual methods, or changing the layout of classes that inline code
  touches, breaks already-built plugins at runtime**, typically as a crash in an unrelated
  call through a shifted vtable slot. Release the engine and every plugin that uses it
  together.
* The same holds for spine's own headers. A change to a spine class used by plugins
  (`HTTP::Request`, `Table`, `Value`, `SmartMetPlugin`, …) needs a spine release and
  rebuilt dependants. Bump the `Requires:` floors in their specs.

## 14. Known pitfalls

* **Restrict administrative plugins with `ip_filters`.** "Private" controls the URI list
  and frontend routing; private handlers additionally default to localhost and the
  private networks, which `ip_filters` replaces, so list every network that needs
  access.
* **`isAdminQuery()` is only a scheduling hint.** It picks the admin thread pool and skips
  the high-load check; it grants and denies nothing.
* **`RequiresAuthentication` admin requests need `admin.user` and `admin.password`**
  in the server configuration; without them `/admin` answers them with 403.
* **Configure `admin.ip_filters` whenever you configure `admin.uri`.**
* **Initialisation is concurrent.** Engines' and plugins' `init()` run in parallel, and a
  plugin that calls an engine method before `getEngine<>()` has returned, or an engine
  that uses shared global state without locking, has a race.
* **Engine constructors are sequential.** Heavy work in an engine constructor delays the
  whole server start.
* **Shutdown has a deadline.** A `shutdown()` that blocks makes the watch thread abort the
  process, which produces a core dump and no clean exit.
