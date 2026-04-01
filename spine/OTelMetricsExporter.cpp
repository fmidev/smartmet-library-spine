#include "OTelMetricsExporter.h"
#include <macgyver/Exception.h>

#ifdef SMARTMET_SPINE_OPENTELEMETRY

// OTel Metrics API
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/metrics/observer_result.h>

// OTel Metrics SDK
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/metric_reader.h>
#include <opentelemetry/sdk/metrics/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>

// OTLP HTTP metrics exporter
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>

namespace
{
// ── Per-metric Observable callbacks ──────────────────────────────────────────
// Each callback receives a void* pointing to the shared ObserveContext.

using Context = SmartMet::Spine::OTelMetricsExporter::ObserveContext;
using Attrs   = std::initializer_list<std::pair<opentelemetry::nostd::string_view,
                                                 opentelemetry::common::AttributeValue>>;

static void observeSize(opentelemetry::metrics::ObserverResult result, void* state)
{
  const auto& stats = static_cast<Context*>(state)->callback();
  for (const auto& [name, s] : stats)
    result.Observe(static_cast<int64_t>(s.size), Attrs{{"cache.name", name}});
}

static void observeMaxSize(opentelemetry::metrics::ObserverResult result, void* state)
{
  const auto& stats = static_cast<Context*>(state)->callback();
  for (const auto& [name, s] : stats)
    result.Observe(static_cast<int64_t>(s.maxsize), Attrs{{"cache.name", name}});
}

static void observeHits(opentelemetry::metrics::ObserverResult result, void* state)
{
  const auto& stats = static_cast<Context*>(state)->callback();
  for (const auto& [name, s] : stats)
    result.Observe(static_cast<int64_t>(s.hits), Attrs{{"cache.name", name}});
}

static void observeMisses(opentelemetry::metrics::ObserverResult result, void* state)
{
  const auto& stats = static_cast<Context*>(state)->callback();
  for (const auto& [name, s] : stats)
    result.Observe(static_cast<int64_t>(s.misses), Attrs{{"cache.name", name}});
}

static void observeInserts(opentelemetry::metrics::ObserverResult result, void* state)
{
  const auto& stats = static_cast<Context*>(state)->callback();
  for (const auto& [name, s] : stats)
    result.Observe(static_cast<int64_t>(s.inserts), Attrs{{"cache.name", name}});
}

}  // namespace

#endif  // SMARTMET_SPINE_OPENTELEMETRY

namespace SmartMet
{
namespace Spine
{

OTelMetricsExporter::OTelMetricsExporter(OTelOptions options, CacheStatsCallback callback)
    : itsOptions(std::move(options)), itsCallback(std::move(callback))
{
}

OTelMetricsExporter::~OTelMetricsExporter()
{
  try
  {
    stop();
  }
  catch (...)
  {
    // destructor must not throw
  }
}

void OTelMetricsExporter::start()
{
  try
  {
#ifdef SMARTMET_SPINE_OPENTELEMETRY
    if (!itsOptions.enabled || itsOptions.metrics_interval_s <= 0)
      return;

    namespace otlp      = opentelemetry::exporter::otlp;
    namespace sdkmet    = opentelemetry::sdk::metrics;
    namespace res       = opentelemetry::sdk::resource;

    // ── OTLP HTTP metrics exporter ────────────────────────────────────────────
    otlp::OtlpHttpMetricExporterOptions exp_opts;
    exp_opts.url     = itsOptions.endpoint + "/v1/metrics";
    exp_opts.timeout = std::chrono::milliseconds(itsOptions.timeout_ms);
    for (const auto& [k, v] : itsOptions.headers)
      exp_opts.http_headers.insert({k, v});

    auto exporter = otlp::OtlpHttpMetricExporterFactory::Create(exp_opts);

    // ── Periodic reader ───────────────────────────────────────────────────────
    sdkmet::PeriodicExportingMetricReaderOptions reader_opts;
    reader_opts.export_interval_millis =
        std::chrono::milliseconds(itsOptions.metrics_interval_s * 1000);
    reader_opts.export_timeout_millis =
        std::chrono::milliseconds(itsOptions.timeout_ms);

    auto reader = sdkmet::PeriodicExportingMetricReaderFactory::Create(
        std::move(exporter), reader_opts);

    // ── Resource ──────────────────────────────────────────────────────────────
    res::ResourceAttributes res_attrs = {
        {res::SemanticConventions::kServiceName,    itsOptions.service_name},
        {res::SemanticConventions::kServiceVersion, itsOptions.service_version},
    };
    auto resource = res::Resource::Create(res_attrs);

    // ── Provider ──────────────────────────────────────────────────────────────
    itsProvider = sdkmet::MeterProviderFactory::Create(
        std::make_unique<sdkmet::ViewRegistry>(), resource);
    itsProvider->AddMetricReader(std::move(reader));

    // ── Observable instruments ────────────────────────────────────────────────
    itsContext = std::make_shared<ObserveContext>(ObserveContext{itsCallback});

    auto meter = itsProvider->GetMeter(itsOptions.service_name, itsOptions.service_version);

    itsSizeGauge    = meter->CreateInt64ObservableGauge(
        "smartmet.cache.size",     "Current number of entries in the cache");
    itsMaxSizeGauge = meter->CreateInt64ObservableGauge(
        "smartmet.cache.max_size", "Configured capacity of the cache");
    itsHitsGauge    = meter->CreateInt64ObservableGauge(
        "smartmet.cache.hits",     "Cumulative cache hits since server start");
    itsMissesGauge  = meter->CreateInt64ObservableGauge(
        "smartmet.cache.misses",   "Cumulative cache misses since server start");
    itsInsertsGauge = meter->CreateInt64ObservableGauge(
        "smartmet.cache.inserts",  "Cumulative cache inserts since server start");

    auto* ctx_raw = itsContext.get();
    itsSizeGauge->AddCallback(observeSize,     ctx_raw);
    itsMaxSizeGauge->AddCallback(observeMaxSize, ctx_raw);
    itsHitsGauge->AddCallback(observeHits,     ctx_raw);
    itsMissesGauge->AddCallback(observeMisses,  ctx_raw);
    itsInsertsGauge->AddCallback(observeInserts, ctx_raw);

    itsIsRunning = true;
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OTelMetricsExporter::start failed");
  }
}

void OTelMetricsExporter::stop()
{
  try
  {
#ifdef SMARTMET_SPINE_OPENTELEMETRY
    if (itsProvider)
    {
      // Drop observable instruments first so callbacks are unregistered
      itsSizeGauge.reset();
      itsMaxSizeGauge.reset();
      itsHitsGauge.reset();
      itsMissesGauge.reset();
      itsInsertsGauge.reset();
      itsContext.reset();

      itsProvider->ForceFlush(std::chrono::milliseconds(itsOptions.timeout_ms));
      itsProvider->Shutdown();
      itsProvider.reset();
    }
#endif
    itsIsRunning = false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OTelMetricsExporter::stop failed");
  }
}

}  // namespace Spine
}  // namespace SmartMet
