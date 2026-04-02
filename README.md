# SmartMet Server

SmartMet Server is a high-performance, high-availability data and product server for MetOcean data, developed and operated by the [Finnish Meteorological Institute (FMI)](https://www.fmi.fi/). Written in C++, it has been in operational use since 2008 and powers the [FMI Open Data Portal](https://en.ilmatieteenlaitos.fi/open-data) since 2013. The server is INSPIRE compliant.

## Capabilities

**Input formats:**
- GRIB 1 and GRIB 2
- NetCDF
- SQL databases
- QueryData (FMI native format)

**Output interfaces and formats:**
- OGC WMS 1.3.0, WFS 2.0
- JSON, XML, ASCII, HTML
- GRIB 1, GRIB 2, NetCDF
- Raster images (PNG, JPEG, SVG)

The server specializes in on-demand extraction and product generation from gridded weather data (GRIB, NetCDF). It is designed for multi-core use, provides LRU caching, and supports frontend/backend load balancing via the Sputnik engine.

## Architecture

![SmartMet Server Structure](https://github.com/fmidev/smartmet-server/blob/master/SmartMet_Structure.png)

The server daemon loads **engines** (shared stateful modules) and **plugins** (HTTP interface handlers) at startup. All components share common **libraries**.

## Components

### Server Daemon
| Repository | Description |
|---|---|
| [smartmet-server](https://github.com/fmidev/smartmet-server) | The server daemon — loads plugins and engines, handles HTTP |

### Libraries
| Repository | Description |
|---|---|
| [smartmet-library-spine](https://github.com/fmidev/smartmet-library-spine) | Core server framework: HTTP handling, plugin/engine management, configuration, thread pool |
| [smartmet-library-newbase](https://github.com/fmidev/smartmet-library-newbase) | Core QueryData format library — the native FMI weather data format with projection and parameter support |
| [smartmet-library-macgyver](https://github.com/fmidev/smartmet-library-macgyver) | General utilities: astronomy, caching, datetime parsing, filesystem, charset conversion, CSV, Base64 |
| [smartmet-library-gis](https://github.com/fmidev/smartmet-library-gis) | GIS operations: coordinate projections, geometry clipping, antimeridian handling, DEM/raster data, PostGIS |
| [smartmet-library-giza](https://github.com/fmidev/smartmet-library-giza) | Color mapping and SVG rendering: palettes, color trees, color-mapped image generation |
| [smartmet-library-locus](https://github.com/fmidev/smartmet-library-locus) | Geographic name and location lookup: geocoding queries with multilingual support |
| [smartmet-library-imagine](https://github.com/fmidev/smartmet-library-imagine) | 2D graphics rendering: Bezier curves, affine transforms, color blending, image compositing |
| [smartmet-library-imagine2](https://github.com/fmidev/smartmet-library-imagine2) | Updated 2D graphics rendering library (successor to imagine) |
| [smartmet-library-calculator](https://github.com/fmidev/smartmet-library-calculator) | Time series and area calculation tools for QueryData |
| [smartmet-library-delfoi](https://github.com/fmidev/smartmet-library-delfoi) | Oracle database access layer for meteorological observations and flash data |
| [smartmet-library-grid-content](https://github.com/fmidev/smartmet-library-grid-content) | Grid support: Content Server, Data Server, and Query Server APIs with Redis, cache, CORBA, and HTTP implementations |
| [smartmet-library-grid-files](https://github.com/fmidev/smartmet-library-grid-files) | Unified driver layer for grid file formats: GRIB 1, GRIB 2, NetCDF, QueryData |
| [smartmet-library-regression](https://github.com/fmidev/smartmet-library-regression) | Regression testing framework for SmartMet tools and libraries |
| [smartmet-library-smarttools](https://github.com/fmidev/smartmet-library-smarttools) | Scripting tools for the SmartMet editor, also used by qdtools |
| [smartmet-library-textgen](https://github.com/fmidev/smartmet-library-textgen) | Algorithms for generating weather forecast text from querydata |
| [smartmet-library-timeseries](https://github.com/fmidev/smartmet-library-timeseries) | Time series data structures and operations |
| [smartmet-library-trajectory](https://github.com/fmidev/smartmet-library-trajectory) | Trajectory calculations for massless particles (used by SmartMet Editor and Server) |
| [smartmet-library-trax](https://github.com/fmidev/smartmet-library-trax) | High-performance marching-squares contouring: isobands and isolines from 2D gridded data |
| [smartmet-library-woml](https://github.com/fmidev/smartmet-library-woml) | Weather Object Model (WOML) file reader, used by the frontier renderer |

### Engines
Engines are shared stateful modules loaded by the server daemon. They are shared across all plugins.

| Repository | Description |
|---|---|
| [smartmet-engine-querydata](https://github.com/fmidev/smartmet-engine-querydata) | QueryData file management and access |
| [smartmet-engine-geonames](https://github.com/fmidev/smartmet-engine-geonames) | Geographic name lookup and geocoding |
| [smartmet-engine-observation](https://github.com/fmidev/smartmet-engine-observation) | Weather station observation data access |
| [smartmet-engine-contour](https://github.com/fmidev/smartmet-engine-contour) | Isoline and isoband contour generation |
| [smartmet-engine-gis](https://github.com/fmidev/smartmet-engine-gis) | GIS data and projection support for plugins |
| [smartmet-engine-sputnik](https://github.com/fmidev/smartmet-engine-sputnik) | Frontend/backend cluster management and load balancing |

### Plugins
Plugins handle HTTP requests and provide the server's external interfaces.

| Repository | Description |
|---|---|
| [smartmet-plugin-timeseries](https://github.com/fmidev/smartmet-plugin-timeseries) | Time series data retrieval (weather parameters at locations over time) |
| [smartmet-plugin-wms](https://github.com/fmidev/smartmet-plugin-wms) | OGC Web Map Service 1.3.0 |
| [smartmet-plugin-wfs](https://github.com/fmidev/smartmet-plugin-wfs) | OGC Web Feature Service 2.0 |
| [smartmet-plugin-download](https://github.com/fmidev/smartmet-plugin-download) | Bulk data download in GRIB, NetCDF, and QueryData formats |
| [smartmet-plugin-frontend](https://github.com/fmidev/smartmet-plugin-frontend) | Load-balancing frontend that distributes requests to backend servers |
| [smartmet-plugin-backend](https://github.com/fmidev/smartmet-plugin-backend) | Backend handler for frontend-routed requests |
| [smartmet-plugin-autocomplete](https://github.com/fmidev/smartmet-plugin-autocomplete) | Location name autocomplete |
| [smartmet-plugin-meta](https://github.com/fmidev/smartmet-plugin-meta) | Metadata about available data and parameters |
| [smartmet-plugin-admin](https://github.com/fmidev/smartmet-plugin-admin) | Server administration interface |

### Tools and Applications
| Repository | Description |
|---|---|
| [smartmet-qdtools](https://github.com/fmidev/smartmet-qdtools) | Comprehensive suite of QueryData handling, conversion, and inspection tools |
| [smartmet-qdcontour](https://github.com/fmidev/smartmet-qdcontour) | Legacy QueryData contouring and map rendering tool |
| [smartmet-qdcontour2](https://github.com/fmidev/smartmet-qdcontour2) | Updated QueryData contouring and map rendering tool |
| [smartmet-shapetools](https://github.com/fmidev/smartmet-shapetools) | Command-line tools for ESRI shapefile operations |
| [smartmet-tools-grid](https://github.com/fmidev/smartmet-tools-grid) | Grid support server, client, and file inspection programs |
| [smartmet-press](https://github.com/fmidev/smartmet-press) | PostScript and ASCII product generation from QueryData |
| [smartmet-fmitools](https://github.com/fmidev/smartmet-fmitools) | FMI-specific meteorological data manipulation tools |
| [smartmet-textgenapps](https://github.com/fmidev/smartmet-textgenapps) | Weather forecast text generation applications |
| [smartmet-frontier](https://github.com/fmidev/smartmet-frontier) | SVG weather chart renderer from WOML input |
| [smartmet-timezones](https://github.com/fmidev/smartmet-timezones) | Timezone data files: Boost.Date_Time database and packed global shapefile |
| [smartmet-fonts](https://github.com/fmidev/smartmet-fonts) | Weather symbol fonts used by SmartMet rendering tools |
| [smartmet-roadindex](https://github.com/fmidev/smartmet-roadindex) | Road weather index calculations |
| [smartmet-roadmodel](https://github.com/fmidev/smartmet-roadmodel) | Road weather model |
| [smartmet-utils](https://github.com/fmidev/smartmet-utils) | General utility scripts |

## smartmet-library-spine

The spine library is the **core framework** of SmartMet Server. It provides:

- **HTTP server** — multi-threaded request handling with a configurable thread pool
- **Plugin manager** — dynamic loading and lifecycle management of plugins
- **Engine manager** — dynamic loading and lifecycle management of shared engines
- **Configuration** — libconfig-based configuration with include support
- **Request/response abstractions** — HTTP request parsing and response building
- **Logging** — structured logging with configurable levels
- **LRU cache** — configurable in-memory response caching

Plugins and engines are built against the spine library API. The spine library itself depends on [smartmet-library-macgyver](https://github.com/fmidev/smartmet-library-macgyver).

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Bug reports and pull requests are welcome on [GitHub](../../issues). For larger contributions, open an issue for discussion first. A CLA is required — contact us for details.

## Contact

- Email: beta@fmi.fi
- GitHub Issues: [issues](../../issues)
- FMI Open Source: https://en.ilmatieteenlaitos.fi/open-source-code
