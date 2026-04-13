# smartmet-library-spine

Part of [SmartMet Server](https://github.com/fmidev/smartmet-server). See the [SmartMet Server documentation](https://github.com/fmidev/smartmet-server) for a full overview of the ecosystem including all engines, plugins, libraries, and tools.

## Overview

The spine library is the **core framework** of SmartMet Server. It provides the foundation that all plugins and engines are built upon.

## Features

- **HTTP server** — multi-threaded request handling with a configurable thread pool
- **Plugin manager** — dynamic loading and lifecycle management of plugins
- **Engine manager** — dynamic loading and lifecycle management of shared engines
- **Configuration** — libconfig-based configuration with include support
- **Request/response abstractions** — HTTP request parsing and response building
- **Logging** — structured logging with configurable levels
- **LRU cache** — configurable in-memory response caching

Plugins and engines are built against the spine library API. The spine library itself depends on [smartmet-library-macgyver](https://github.com/fmidev/smartmet-library-macgyver).

## Documentation

- [OpenTelemetry support](docs/build-opentelemetry.md) — distributed traces and cache-statistics metrics

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Bug reports and pull requests are welcome on [GitHub](../../issues). For larger contributions, open an issue for discussion first. A CLA is required — contact us for details.

## Contact

- Email: beta@fmi.fi
- GitHub Issues: [issues](../../issues)
