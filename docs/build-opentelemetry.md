# Building opentelemetry-cpp locally

No official `opentelemetry-cpp-devel` RPM exists for Rocky/RHEL 10 yet
(the installed `opentelemetry-collector` package is the *collector daemon*,
not the C++ instrumentation library).

Until an RPM is available the recommended path is to build the library once
and install it into `third_party/opentelemetry-cpp/install/` inside this repo.
The Makefile detects that directory automatically.

## Prerequisites

```bash
sudo dnf install -y \
    cmake ninja-build gcc-c++ \
    protobuf-devel grpc-devel \
    curl-devel zlib-devel \
    git
```

`grpc-devel` is optional; omit `-DWITH_OTLP_GRPC=ON` below if it is not
available and use HTTP-only export instead.

## Clone and build

```bash
cd /path/to/spine          # this repository root

mkdir -p third_party
git clone --depth 1 --branch v1.17.0 \
    https://github.com/open-telemetry/opentelemetry-cpp.git \
    third_party/opentelemetry-cpp

cd third_party/opentelemetry-cpp
git submodule update --init --recursive

cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=install \
    -DWITH_OTLP=ON \
    -DWITH_OTLP_HTTP=ON \
    -DWITH_OTLP_GRPC=OFF \
    -DBUILD_TESTING=OFF \
    -DWITH_EXAMPLES=OFF \
    -DWITH_BENCHMARK=OFF \
    -GNinja

cmake --build build -j$(nproc)
cmake --install build
```

After this, the headers land in `third_party/opentelemetry-cpp/install/include/`
and libraries in `third_party/opentelemetry-cpp/install/lib{,64}/`.

## Verify detection

```bash
cd /path/to/spine
make clean && make
```

You should see `-DSMARTMET_SPINE_OPENTELEMETRY` in the compiler command lines
and no link errors.

## Packaging as an RPM (future)

When an `opentelemetry-cpp-devel` RPM becomes available in Fedora / EPEL,
add the following to `smartmet-library-spine.spec`:

```spec
BuildRequires: opentelemetry-cpp-devel
Requires:      opentelemetry-cpp
```

and remove or keep the local-build fallback in the Makefile as desired.

## Runtime configuration

Add an `opentelemetry` group to `smartmet.conf`:

```
opentelemetry:
{
  enabled        = true;

  # OTLP/HTTP endpoint (the opentelemetry-collector listens here by default)
  endpoint       = "http://localhost:4318";

  service_name   = "smartmet-server";
  service_version = "1.0";

  # Export timeout in milliseconds
  timeout_ms     = 10000;

  # true  → BatchSpanProcessor (recommended for production)
  # false → SimpleSpanProcessor (useful for debugging; blocks on network I/O)
  batch          = true;

  # BatchSpanProcessor tuning
  batch_max_queue_size    = 2048;
  batch_schedule_delay_ms = 5000;

  # How often to push cache-statistics metrics (seconds).
  # Set to 0 to disable metrics export while keeping trace export.
  metrics_interval_s = 60;

  # Optional HTTP headers for auth (e.g. New Relic / Honeycomb / OTEL Collector)
  headers = { Authorization = "Bearer <api-key>"; };
};
```

---

## Distributed tracing (HTTP request spans)

Each registered URI handler gets its own OTel `Tracer`.  Every completed
HTTP request produces one finished span exported to `{endpoint}/v1/traces`.

Attributes follow OTel Semantic Conventions ≥ 1.23:

| Attribute | Source |
|---|---|
| `http.request.method` | request method |
| `url.path` | request URI |
| `http.response.status_code` | HTTP status code (int) |
| `client.address` | client IP |
| `http.response.body.size` | response content-length |
| `http.route` | registered handler URI |
| `network.protocol.version` | HTTP version |
| `http.response.header.etag` | ETag (when present) |
| `service.name` | from config |
| `service.version` | from config |

Spans with status ≥ 500 are marked `ERROR`; all others are `OK`.
Start and end timestamps are taken directly from `LoggedRequest`, so
spans land at the correct wall-clock position even though they are
exported after the request has finished.

OTel tracing is independent of the file-based access log toggle
(`defaultlogging` / `/admin?what=logging`).  If OTel is enabled, timing
measurements are always collected regardless of the file-logging state.

---

## Cache-statistics metrics

`Reactor::getCacheStats()` aggregates hit/miss/insert counters and current
sizes from all loaded engines and plugins.  When `metrics_interval_s > 0`,
a `PeriodicExportingMetricReader` (internal SDK thread) calls the registered
Observable Gauge callbacks on each interval and pushes the snapshot to
`{endpoint}/v1/metrics`.

Published metrics (all carry the attribute `cache.name` = cache identifier):

| Metric | Type | Description |
|---|---|---|
| `smartmet.cache.size` | gauge | Current number of cached entries |
| `smartmet.cache.max_size` | gauge | Configured capacity |
| `smartmet.cache.hits` | gauge | Cumulative cache hits since server start |
| `smartmet.cache.misses` | gauge | Cumulative cache misses since server start |
| `smartmet.cache.inserts` | gauge | Cumulative inserts since server start |

Note: hits/misses/inserts are cumulative counters exposed as gauges because
`CacheStats` does not reset them; backends (e.g. Prometheus) that need a
rate should use `rate()` or `increase()` on the scraped values.

The metrics exporter lifecycle is tied to `Reactor`; it starts after all
engines and plugins are loaded (so all caches are registered) and stops
during server shutdown.
