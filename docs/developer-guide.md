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

## 3. Source overview

| Area | Files |
|------|-------|
| Core | `Reactor`, `ContentHandlerMap`, `HandlerView`, `DynamicPlugin`, `SmartMetEngine`, `SmartMetPlugin`, `SmartMet.h` (API version), `Options`, `Names`, `Thread` |
| HTTP | `HTTP`, `HTTPParsers`, `HTTPAuthentication`, `IPFilter`, `FmiApiKey`, `ActiveRequests`, `ActiveBackends`, `LoggedRequest`, `AccessLogger`, `LogRange` |
| Output | `Table`, `TableFormatter` and its subclasses, `TableFormatterFactory`, `TableFormatterOptions`, `TableVisitor`, `Value`, `None` |
| Configuration | `ConfigBase`, `ConfigTools`, `Convenience` |
| Weather domain | `Parameter`, `Parameters`, `ParameterTranslations`, `Location`, `Station`, `LonLat`, `QCConverter`, `MultiLanguageString(Array)`, `Translations`, `StringTranslations` |
| Caches | `SmartMetCache`, `FileCache`, `JsonCache` |
| Other | `CRSRegistry`, `HostInfo`, `TcpMultiQuery`, `Json`, `Backtrace`, `MallocStats`, `Exceptions`, `OTel*` |

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
have heard about. A **private** handler is therefore not reachable **through a
frontend**, but it answers anyone who can connect to the backend's own port directly.
To restrict it, configure `plugins.<name>.ip_filters` (checked by the handler view) or
authenticate in the plugin.

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
| `RequiresAuthentication` | HTTP Basic authentication with `admin.user` / `admin.password` from the server configuration. **If those are not configured, the registration is silently ignored**, so the request does not exist. |

The admin URI is set by `admin.uri`. If `admin.uri` is not configured, the admin
requests are restricted to `127.0.0.1` unless `admin.ip_filters` says otherwise. When
`admin.uri` is configured without `admin.ip_filters`, there is no IP restriction.

The Reactor registers its own requests: `list`, `lastrequests`, `activerequests`,
`cachestats`, `servicestats`, `engineinfo`, `plugininfo`, `waitforready`,
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
  It does not handle IPv6 addresses such as the loopback `::1`.
* **`HostInfo`** resolves client host names (cached, in background threads, controlled
  by the `clientHostName*` options). **`TcpMultiQuery`** sends several TCP queries in
  parallel; the frontend uses it to query its backends' state.
* **Active requests and access logs.** `ActiveRequests` tracks running requests
  (`what=activerequests`); a terminate handler prints them if the process dies.
  `AccessLogger` writes per-URI access logs when logging is on.
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

* **"Private" does not mean protected.** A private content handler is hidden from the URI
  list and from the frontends, but it is fully reachable on the backend port. Use
  `ip_filters` or authentication for administrative plugins.
* **`isAdminQuery()` is only a scheduling hint.** It picks the admin thread pool and skips
  the high-load check; it grants and denies nothing.
* **`RequiresAuthentication` admin requests vanish without `admin.user`/`admin.password`.**
  The registration returns success but the request is never added.
* **Configuring `admin.uri` removes the localhost default.** Without `admin.ip_filters`,
  the admin requests are then open to every client.
* **`IPFilter` has no IPv6 support** (not even `::1`).
* **Initialisation is concurrent.** Engines' and plugins' `init()` run in parallel, and a
  plugin that calls an engine method before `getEngine<>()` has returned, or an engine
  that uses shared global state without locking, has a race.
* **Engine constructors are sequential.** Heavy work in an engine constructor delays the
  whole server start.
* **Shutdown has a deadline.** A `shutdown()` that blocks makes the watch thread abort the
  process, which produces a core dump and no clean exit.
