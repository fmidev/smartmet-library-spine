# smartmet-library-spine — Feature List

A structured inventory of capabilities provided by the spine library.
Use as a checklist when drafting release notes. When new functionality
is added, append the new entry under the matching section (and bump
the *Last updated* line at the bottom).

`smartmet-library-spine` is the **core framework** of SmartMet Server.
Every plugin and engine in the ecosystem is built on top of it. It
provides the HTTP server, the dynamic plugin / engine loader, the
configuration system, request/response abstractions, output
formatters, and a multi-tier cache. All code lives under the
`SmartMet::Spine` namespace; the library produces
`libsmartmet-spine.so` plus the auxiliary `smartmet-plugin-test`
binary used by every plugin's integration-test harness.

---

## 1. Reactor & server core

- **`Reactor`** — central server object. Owns the entire plugin /
  engine lifecycle: loads `.so` files via `dlopen`, initialises them
  in async task groups, manages shutdown.
- **Singleton access** — `Reactor::instance` is the canonical handle.
- **Plugin / engine factory pattern** — plugins export `create` /
  `destroy` C symbols, resolved by `DynamicPlugin`; engines export
  `engine_name` / `engine_class_creator`, resolved by the `Reactor`.
- **`SmartMetPlugin`** — abstract base for plugins; concrete plugins
  implement `init()`, `shutdown()`, `requestHandler()`.
- **`SmartMetEngine`** — abstract base for engines; concrete engines
  implement `init()` and `shutdown()`.
- **`Reactor::getEngine<T>()`** — type-safe engine lookup used by
  plugins to retrieve their dependencies.
- **`DynamicPlugin`** — wraps the loaded `.so`, manages symbol
  resolution and lifecycle.

## 2. URL routing & content handlers

- **`ContentHandlerMap`** — URI-to-handler routing table; `Reactor`
  inherits from it.
- **`HandlerView`** — per-handler view of routes registered by one
  plugin.
- **Admin endpoint registration** — `bool`, `Table`, string and custom
  handler variants, each with an access level (`Public`, `Private`,
  `RequiresAuthentication`).
- **Public vs private routes** — private handlers are not advertised
  to the frontends and accept only local and private networks unless
  the plugin has `ip_filters`; the admin URI is gated by
  `admin.ip_filters`.

## 3. HTTP layer

- **`HTTP::Request` / `HTTP::Response`** — request/response data
  classes with case-insensitive header and query maps.
- **`HTTPParsers`** — wire-protocol parsers.
- **`HTTP::ContentStreamer`** — streaming response interface for
  large or chunked responses (used by the download and WMS plugins).
- **`HTTPAuthentication`** — HTTP Basic authentication helpers.
- **`FmiApiKey`** — FMI-style API-key extraction from headers / query
  string.
- **`IPFilter`** — IP-based access control (range / mask matching),
  used for plugin `ip_filters` and the admin URI.

## 4. Configuration

- **`ConfigBase`** — libconfig wrapper with typed accessors and
  consistent error handling.
- **`ConfigTools`** — additional config helpers and validation.
- **SmartMet libconfig extensions** — `@include` (libconfig) and
  `$(VAR)` expansion from other settings or the environment, plus
  host-specific settings (`lookupHostSetting`).
- **`Options`** — server-startup configuration from command-line +
  config file:
  - Port, TLS settings.
  - Thread pools: `adminpool`, `slowpool`, `fastpool`
    (`maxthreads`, `maxrequeuesize`).
  - Throttle configuration (`activerequests.*`).
  - Client host name resolution (`dns.*`).
  - Logging configuration.
  - Compression.
  - Cache-control headers (`staleWhileRevalidate`, `staleIfError`).
  - OpenTelemetry options.

## 5. Output formatters

All `TableFormatter`-derived; all but `ImageFormatter` are created
via `TableFormatterFactory`:

- **`AsciiFormatter`** — plain text tables.
- **`CsvFormatter`** — comma-separated values.
- **`DebugFormatter`** — diagnostic dump.
- **`HtmlFormatter`** — HTML tables.
- **`ImageFormatter`** — HTML table like `DebugFormatter`, but the
  cell values are not escaped (for example `<img>` tags).
- **`JsonFormatter`** — JSON output.
- **`PhpFormatter`** — PHP array literal syntax.
- **`SerialFormatter`** — PHP `serialize()` format.
- **`WxmlFormatter`** — FMI weather XML.
- **`XmlFormatter`** — generic XML.
- **`TableFormatterOptions`** — per-format options (precision,
  missing-text, separator, etc.).
- **`TableVisitor`** — visitor pattern over table rows.

## 6. Caching

- **`SmartMetCache`** — two-tier cache:
  - **Memory cache** via `Fmi::Cache::Cache` (CLOCK eviction) with
    `NumShards=1`, so that byte-based sizing gives one value the whole
    capacity.
  - **Filesystem cache** for evicted entries; eviction → disk happens
    asynchronously on a background thread.
- **`JsonCache`** — cache of parsed JSON files, reloaded when a file
  changes.
- **`FileCache`** — standalone filesystem cache.
- **`Table`** — in-memory tabular result type that formatters
  consume.

## 7. Logging & instrumentation

- **`AccessLogger`** — per-request access log with configurable
  format.
- **`LoggedRequest`** — structured access-log entry.
- **`LogRange`** — query range from the access log.
- **`ActiveRequests`** — registry of currently in-flight requests
  (for admin views).
- **`ActiveBackends`** — currently-connected backends.
- **`MallocStats`** — runtime memory statistics.
- **`HostInfo`** — client-IP reverse-DNS (PTR) resolution for
  diagnostics / the admin active-requests view. Cached (LRU +
  positive/negative TTL) and resolved entirely on background worker
  threads: the request thread only `prefetch()`es the IP and output
  consumers read the cache via `getHostName()`, so a slow or missing
  PTR record can never stall request handling. Controlled via the
  `dns` config group (`dns.resolve`, `dns.cachesize`, `dns.positivettl`,
  `dns.negativettl`, `dns.threads`, `dns.maxqueuesize`). Its LRU cache is reported in the
  server's overall cache-statistics output (`getCacheStats()`) as
  `Spine::HostInfo::dns_cache`.
- **`Backtrace`** — runtime stack-trace capture, used by the
  `Fmi::Exception` family on crashes.
- **`Exceptions`** — spine-specific exception types over
  `Fmi::Exception`.

## 8. Cluster & backend support

- **`TcpMultiQuery`** — parallel TCP queries to multiple backends
  (used by the frontend plugin to fan out to backend servers).
- **`ActiveBackends`** — broadcast / track which backend instances
  are currently alive (works with the sputnik engine + frontend
  plugin).

## 9. Meteorology helpers

- **`Parameter`** — meteorological parameter with id, name, units.
- **`Parameters`** — collection of `Parameter`.
- **`ParameterTranslations`** — locale-aware parameter labels.
- **`MultiLanguageString`** / **`MultiLanguageStringArray`** —
  translatable strings used in formatted output.
- **`StringTranslations`** / **`Translations`** — translation table
  helpers.
- **`Names`** — translatable name registry.
- **`Station`** — observation station metadata.
- **`Location`**, **`LonLat`** — point location types.
- **`QCConverter`** — quality-code conversion helper.

## 10. Spatial reference

- **`CRSRegistry`** — CRS registry on top of GDAL / proj:
  - Lookup by EPSG, name, WKT.
  - Per-CRS axis swap / wraparound rules.
  - Custom CRS definitions from the config.
- **Test fixtures** — `test/crs/` holds reference data for the
  registry tests.

## 11. JSON utilities

- **`Json`** — high-level JSON helpers built on jsoncpp.
- **`JsonCache`** — parsed-JSON cache.
- **`Value`** — typed request-parameter value (bool, integers,
  double, string, time, point, bounding box) with parsing and checked
  conversions; used by the WFS stored queries.

## 12. OpenTelemetry (optional)

Enabled by defining `SMARTMET_SPINE_OPENTELEMETRY` at compile time:

- **`OTelOptions`** — config block for tracing / metrics endpoints
  and sampling.
- **`OTelLogger`** — span / log emission.
- **`OTelMetricsExporter`** — exports cache stats and runtime
  metrics.
- **Documentation**: `docs/build-opentelemetry.md`.

## 13. OpenAPI documentation, API catalog & data explorer

- **`OpenAPI::Publisher`** — publishes the OpenAPI 3.1 definition of a
  plugin at `<root>/api` and Swagger UI at `<root>/api/docs`, with the
  SmartMet Swagger UI plugin (code snippets, response tree, maps).
- **`OpenAPI::Catalog`** (RFC 9727) — `/.well-known/api-catalog`,
  `/api` (linkset or HTML) and `/api/docs` with every API of the
  server, or of the cluster on a frontend (`parseCatalog()`), APIs
  carrying their kind as `x-smartmet-kind`.
- **Data explorer** at `/api/explorer` — queries built from the
  metadata of the TimeSeries, EDR and Coverages APIs, with a Browse
  page of all their datasets, maps, charts, soundings, sections,
  meteograms, ensembles, linked Coverages maps, units, a 12/24-hour
  clock and English, Finnish and Swedish texts.
- **`api_catalog.explorer_defaults`** — the first map view, the
  location examples, extra output projections, the language, the
  clock and the units for the region of a server.
- **Documentation**: `docs/developer-guide.md` §11.

## 14. Convenience & misc

- **`Convenience`** — shared helpers used across plugins (string
  trims, time conversions, …).
- **`None`** — explicit "absent" marker.
- **`Thread`** — thread-related helpers.
- **`SmartMet`** — top-level umbrella header.

## 15. `app/` — auxiliary binaries

Two helper programs are built alongside the library:

- **`smartmet-plugin-test`** (`SmartmetPluginTest.cpp` +
  `PluginTest.cpp`) — the integration-test runner used by every
  plugin's `test/` suite. Boots a `Reactor` with a configured plugin,
  replays `*.get` request files, captures responses.
- **`cfgvalidate`** — libconfig validation tool used by every
  plugin's `make configtest` target.

## 16. Testing

- **~34 standalone test executables** under `test/`, one per
  `*Test.cpp`.
- **Framework**: Boost.Test.
- **Reactor integration tests** under `test/reactor_tests/` — boot
  a `Reactor` with a plugin loaded; driven by
  `app/smartmet-plugin-test`.
- **CRS test data** under `test/crs/`.
- **Sanitiser builds**:
  - `make -C test ASAN=yes test` — address + UB sanitiser.
  - `make -C test TSAN=yes test` — thread sanitiser.

## 17. Build & integration

- **Library output**: `libsmartmet-spine.so` (also installs headers).
- **Build**: `make` (release) — also builds `app/` binaries.
- **Format**: `make format` runs clang-format (Google-based, Allman
  braces, 100-col).
- **Install**: `make install`.
- **RPM**: `make rpm`.
- **CI**: CircleCI on RHEL 8 / RHEL 10 (`fmidev/smartmet-cibase-{8,10}`
  Docker images) via the standard `ci-build` workflow.
- **Common Makefile bits**: `common.mk` is shared by the library,
  `app/`, and `test/` builds; `smartbuildcfg` provides `makefile.inc`.
- **pkg-config requirements**: `gdal`, `jsoncpp`, `mariadb`,
  `icu-i18n`, `configpp`.
- **Boost components linked**: `thread`, `regex`, `program_options`,
  `locale`, `asio`, `chrono`, `timer`.
- **Other libraries**: ctpp2 (templates), fmt 12.x,
  `smartmet-library-macgyver`, `smartmet-library-newbase`.
- **Public headers** installed under `/usr/include/smartmet/spine/`.

---

*Last updated: 2026-10-17.*
