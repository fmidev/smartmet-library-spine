# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`smartmet-library-spine` is the core framework library of SmartMet Server. It provides the HTTP server foundation, dynamic plugin/engine loading, configuration, request/response abstractions, output formatting, and caching. All SmartMet plugins and engines depend on this library.

Namespace: `SmartMet::Spine`. Produces `libsmartmet-spine.so`. Headers install to `smartmet/spine/`.

## Build commands

```bash
make                  # Build the library (libsmartmet-spine.so) and app/
make test             # Run all unit tests + reactor integration tests
make format           # clang-format all source (Google-based, Allman braces, 100-col)
make clean            # Clean build artifacts
make rpm              # Build RPM package
```

Build requires `smartbuildcfg` to be installed (provides `makefile.inc`). Dependencies via pkg-config: `gdal jsoncpp mariadb icu-i18n configpp`.

### Running a single test

Tests are individual executables in `test/`. Each `*Test.cpp` compiles to a standalone Boost.Test binary:

```bash
make -C test SmartMetCacheTest    # Build one test
./test/SmartMetCacheTest          # Run it
```

Reactor integration tests (test a plugin loaded into a live Reactor) live in `test/reactor_tests/` and use `smartmet-plugin-test` (built from `app/`):

```bash
make -C test/reactor_tests        # Build test engine + plugin
make -C test/reactor_tests test   # Run reactor test with app/smartmet-plugin-test
```

### Sanitizers

```bash
make -C test TSAN=yes test        # Thread sanitizer
make -C test ASAN=yes test        # Address + UB sanitizer
```

## Architecture

### Core classes

- **`Reactor`** (`Reactor.h`) — the central server object. Owns the plugin/engine lifecycle: loads `.so` files via `dlopen`, initializes them in async task groups, manages shutdown. Inherits from `ContentHandlerMap`. Singleton accessed via `Reactor::instance`. Plugins obtain engine references through `Reactor::getEngine<T>()`.

- **`ContentHandlerMap`** (`ContentHandlerMap.h`) — URI-to-handler routing. Maps request paths to `HandlerView` entries associated with plugins. Also handles admin request registration (both bool and table-returning variants).

- **`SmartMetPlugin`** / **`SmartMetEngine`** — abstract base classes that all plugins/engines must inherit. Plugins implement `init()`, `shutdown()`, `requestHandler()`. Engines implement `init()`, `shutdown()`. Both use `dlopen`-based factory functions (`plugin_create_t`, `plugin_destroy_t`).

- **`DynamicPlugin`** — wraps a loaded plugin `.so`: resolves `create`/`destroy` symbols, manages the plugin lifecycle.

- **`Options`** (`Options.h`) — server startup configuration parsed from command-line args and libconfig file. Includes port, thread pools (`adminpool`/`slowpool`/`fastpool`), throttle settings, TLS, logging, compression, cache-control headers (`staleWhileRevalidate`, `staleIfError`), and OpenTelemetry options.

### HTTP layer

- **`HTTP::Request` / `HTTP::Response`** (`HTTP.h`) — request/response types. Headers and query parameters use case-insensitive string maps.
- **`HTTP::ContentStreamer`** — streaming response interface used for large/chunked responses.

### Output formatting

`TableFormatter` is the abstract base; concrete implementations format `Table` data into different output formats:
`AsciiFormatter`, `CsvFormatter`, `JsonFormatter`, `XmlFormatter`, `WxmlFormatter`, `HtmlFormatter`, `PhpFormatter`, `SerialFormatter`, `DebugFormatter`, `ImageFormatter`. Created via `TableFormatterFactory`.

### Caching

- **`SmartMetCache`** — two-tier cache (memory LRU + filesystem). Memory cache uses `Fmi::Cache::Cache` with `NumShards=1` (byte-based sizing requires deterministic LRU). Evicted entries are written to the file cache asynchronously via a background thread.
- **`JsonCache`** — specialized cache for JSON responses.
- **`FileCache`** — standalone filesystem cache.

### Other notable components

- **`ConfigBase` / `ConfigTools`** — libconfig wrappers with typed accessors and error handling.
- **`CRSRegistry`** — coordinate reference system registry (projections via GDAL/Proj).
- **`IPFilter`** — IP-based access control for admin endpoints.
- **`Parameter` / `Parameters`** — meteorological parameter definitions and translations.
- **`TcpMultiQuery`** — parallel TCP queries to multiple backends.
- **`OTel*`** — optional OpenTelemetry integration (tracing, metrics); enabled by defining `SMARTMET_SPINE_OPENTELEMETRY` at compile time.

## Key dependencies

- `smartmet-library-macgyver` — utilities, `Fmi::Cache`, `Fmi::Exception`, `Fmi::AsyncTaskGroup`, `Fmi::DateTime`
- `smartmet-library-newbase` — QueryData format (linked but not heavily used directly in spine)
- Boost (thread, regex, program_options, locale, asio, chrono, timer)
- libconfig (`configpp`) — configuration files
- GDAL — coordinate reference systems
- jsoncpp — JSON handling
- ctpp2 — template engine (used by formatters)
- fmt 12.x

## CI

CircleCI builds and tests on RHEL 8 and RHEL 10 using `fmidev/smartmet-cibase-{8,10}` Docker images. The `ci-build` tool handles dependency installation, RPM building, and test execution.
