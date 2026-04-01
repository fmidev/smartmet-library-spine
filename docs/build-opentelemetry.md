# OpenTelemetry support in SmartMet Server

SmartMet Server can export two kinds of telemetry via the
[OpenTelemetry](https://opentelemetry.io/) standard:

- **Distributed traces** — one span per HTTP request, sent to a trace backend
  such as Jaeger, Grafana Tempo, or a SaaS vendor.
- **Cache-statistics metrics** — periodic gauges for every engine/plugin cache,
  sent to a metrics backend such as Prometheus or InfluxDB.

Both use the **OTLP/HTTP** protocol
([spec](https://opentelemetry.io/docs/specs/otlp/)) and are typically received
by an [OpenTelemetry Collector](https://opentelemetry.io/docs/collector/) that
fans out to one or more storage backends.  No direct database driver is
required inside SmartMet Server; backend selection is entirely a Collector
configuration concern.

---

## Quick start

1. Install or build the C++ SDK (see [Building the SDK](#building-the-sdk)).
2. Run or point to an OpenTelemetry Collector that accepts OTLP/HTTP on port 4318.
3. Add an `opentelemetry` block to `smartmet.conf` (see [Configuration reference](#configuration-reference)).
4. Restart SmartMet Server.

---

## Supported backends (via the Collector)

The Collector ships exporters for, among others:

| Category | Examples |
|---|---|
| Trace stores | Jaeger, Zipkin, Grafana Tempo, AWS X-Ray, Google Cloud Trace |
| Metrics stores | Prometheus, InfluxDB, Grafana Mimir, AWS CloudWatch |
| Columnar databases | ClickHouse (official OTel exporter) |
| Search / log stores | Elasticsearch, OpenSearch, Grafana Loki |
| Observability SaaS | Datadog, New Relic, Honeycomb, Dynatrace, Splunk |

The Collector can fan out to multiple backends simultaneously.
See the [Collector exporters registry](https://opentelemetry.io/ecosystem/registry/?component=exporter)
for a full, up-to-date list.

---

## Configuration reference

Add an `opentelemetry` group anywhere at the top level of `smartmet.conf`.
All settings are optional; the defaults are shown below.

```
opentelemetry:
{
  enabled             = false;
  endpoint            = "http://localhost:4318";
  service_name        = "smartmet-server";
  service_version     = "";
  timeout_ms          = 10000;
  batch               = true;
  batch_max_queue_size    = 2048;
  batch_schedule_delay_ms = 5000;
  metrics_interval_s  = 60;
  headers             = {};
};
```

### `enabled` (bool, default `false`)

Master switch.  When `false` the entire OpenTelemetry subsystem is inactive:
no spans are created, no metrics are collected, and no network connections are
made.  All other settings are ignored.

### `endpoint` (string, default `"http://localhost:4318"`)

Base URL of the OTLP/HTTP receiver, **without** a path suffix.  The library
appends `/v1/traces` for trace export and `/v1/metrics` for metrics export.

Typical values:
- `"http://localhost:4318"` — local OpenTelemetry Collector (default port)
- `"https://otlp.nr-data.net"` — New Relic direct ingest
- `"https://api.honeycomb.io"` — Honeycomb direct ingest
- `"http://otel-collector.internal:4318"` — Collector on a separate host

Use `http://` for unencrypted traffic to a local Collector.  Use `https://`
when sending directly to a cloud vendor or when the Collector is on a remote
host and TLS is required.

### `service_name` (string, default `"smartmet-server"`)

Human-readable name that identifies this service in trace and metric backends.
Appears as the `service.name` resource attribute on every span and metric data
point.  Use a consistent name across all SmartMet Server instances so they
group together in dashboards and trace search.

### `service_version` (string, default `""`)

Version string attached as `service.version` to all telemetry.  Useful for
correlating incidents with deployments.  Can be set to the RPM version, a git
tag, or left empty.

### `timeout_ms` (int, default `10000`)

Maximum time in milliseconds the exporter waits for the Collector to
acknowledge a single export request.  Applies to both trace and metrics
exports, and also to the final flush on server shutdown.  If the Collector
does not respond within this time the export is silently dropped — telemetry
export is always best-effort and never blocks request handling.

### `batch` (bool, default `true`)

Controls the span processor used for trace export:

| Value | Processor | Behaviour |
|---|---|---|
| `true` | `BatchSpanProcessor` | Spans are queued in memory and flushed periodically or when the queue fills. **Recommended for production.** Export happens on a background thread. |
| `false` | `SimpleSpanProcessor` | Each span is exported synchronously on the thread that calls `log()`. Blocks under the logging mutex until the HTTP round-trip completes or times out. Useful only for debugging with a local Collector. |

### `batch_max_queue_size` (int, default `2048`)

Maximum number of finished spans that the `BatchSpanProcessor` holds in its
internal queue while waiting for the next export cycle.  When the queue is
full, newly finished spans are **dropped** (not back-pressured) to protect
request-handling threads.  Increase this value if you observe dropped spans
at high request rates.  Has no effect when `batch = false`.

### `batch_schedule_delay_ms` (int, default `5000`)

How long (in milliseconds) the `BatchSpanProcessor` waits between export
attempts.  Shorter values reduce end-to-end latency in the trace backend at
the cost of more frequent network round-trips.  A value of `5000` means spans
may appear in the backend up to 5 seconds after the request finished.  Has no
effect when `batch = false`.

### `metrics_interval_s` (int, default `60`)

How often (in seconds) the cache-statistics gauges are pushed to the Collector.
The export is performed by a background thread (the SDK's
`PeriodicExportingMetricReader`); it does not affect request handling.

Set to `0` to disable metrics export entirely while keeping trace export
active.  The metrics exporter does not start until all engines and plugins have
finished loading, so the first export fires at most `metrics_interval_s`
seconds after server initialisation completes.

### `headers` (group, default empty)

A libconfig group of key–value pairs that are sent as HTTP headers on every
OTLP export request.  Use this for authentication when sending directly to a
cloud vendor without a local Collector:

```
headers:
{
  # New Relic
  "api-key" = "YOUR_LICENSE_KEY";

  # Honeycomb
  "x-honeycomb-team"    = "YOUR_API_KEY";
  "x-honeycomb-dataset" = "smartmet";

  # Generic Bearer token
  Authorization = "Bearer YOUR_TOKEN";
};
```

When using a local OpenTelemetry Collector this section is typically empty;
authentication to the upstream vendor is handled in the Collector's own
configuration.

---

## Exported telemetry

### HTTP request spans (traces)

Each registered URI handler has its own `Tracer`.  Every completed HTTP
request produces one finished span exported to `{endpoint}/v1/traces`.

The span name is `METHOD /route`, e.g. `GET /wfs`.

Attributes follow [OTel HTTP Semantic Conventions ≥ 1.23](https://opentelemetry.io/docs/specs/semconv/http/http-spans/):

| Attribute | Type | Source |
|---|---|---|
| `service.name` | string | `service_name` config |
| `service.version` | string | `service_version` config |
| `http.route` | string | registered handler URI |
| `http.request.method` | string | request method |
| `url.path` | string | full request URI |
| `network.protocol.version` | string | HTTP version |
| `client.address` | string | client IP address |
| `http.response.status_code` | int | HTTP status code |
| `http.response.body.size` | int | response content-length (omitted if 0) |
| `http.response.header.etag` | string | ETag header (omitted if absent) |

Span status: `ERROR` for HTTP 5xx responses, `OK` for everything else.

Start and end timestamps come directly from `LoggedRequest`, so spans reflect
the actual wall-clock request duration even though they are exported
asynchronously after the request has finished.

OTel tracing is **independent** of the file-based access-log toggle
(`defaultlogging` / `/admin?what=logging`).  Timing is always measured when
OTel is enabled, even if file logging is off.

### Cache-statistics metrics

`Reactor::getCacheStats()` aggregates counters from all loaded engines and
plugins.  The metrics are pushed to `{endpoint}/v1/metrics` every
`metrics_interval_s` seconds.

All metrics carry a `cache.name` attribute that identifies the cache by the
key returned by the engine or plugin.

| Metric name | Unit | Description |
|---|---|---|
| `smartmet.cache.size` | entries | Current number of cached entries |
| `smartmet.cache.max_size` | entries | Configured maximum capacity |
| `smartmet.cache.hits` | count | Cumulative hits since server start |
| `smartmet.cache.misses` | count | Cumulative misses since server start |
| `smartmet.cache.inserts` | count | Cumulative inserts since server start |

All five are reported as Observable Gauges.  Hits, misses, and inserts are
cumulative (they are never reset) — backends that need a rate should apply
a `rate()` or `increase()` function over the scraped values.

---

## Building the SDK

No official `opentelemetry-cpp-devel` RPM exists for Rocky/RHEL 10 yet.
The installed `opentelemetry-collector` RPM is the *Collector daemon*, not the
C++ instrumentation library.

The Makefile detects the SDK in two places, in order:
1. System-wide via `pkg-config opentelemetry-cpp` (future RPM).
2. A local build in `third_party/opentelemetry-cpp/install/` inside this repo.

### Prerequisites for a local build

```bash
sudo dnf install -y \
    cmake ninja-build gcc-c++ \
    protobuf-devel \
    curl-devel zlib-devel \
    git
```

`grpc-devel` is only needed for gRPC transport (not used here; we use HTTP).

### Clone and build

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

Headers land in `third_party/opentelemetry-cpp/install/include/` and
libraries in `third_party/opentelemetry-cpp/install/lib{,64}/`.

### Verify detection

```bash
make clean && make
```

The compiler lines should contain `-DSMARTMET_SPINE_OPENTELEMETRY` and the
link step should produce no unresolved symbol errors.

### Packaging as an RPM (future)

When `opentelemetry-cpp-devel` becomes available in Fedora / EPEL, add to
`smartmet-library-spine.spec`:

```spec
BuildRequires: opentelemetry-cpp-devel
Requires:      opentelemetry-cpp
```

and remove or keep the local-build fallback in the Makefile as desired.

---

## Further reading

- [OpenTelemetry C++ SDK](https://opentelemetry.io/docs/languages/cpp/) —
  API reference, advanced configuration, and custom exporters.
- [OTLP specification](https://opentelemetry.io/docs/specs/otlp/) —
  wire protocol details.
- [OTel Collector](https://opentelemetry.io/docs/collector/) —
  how to route telemetry to multiple backends, add processors, and filter data.
- [HTTP Semantic Conventions](https://opentelemetry.io/docs/specs/semconv/http/http-spans/) —
  full list of standard HTTP span attributes.
- [Collector exporters registry](https://opentelemetry.io/ecosystem/registry/?component=exporter) —
  current list of supported storage and SaaS backends.
