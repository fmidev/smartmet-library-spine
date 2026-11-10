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
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/semconv/service_attributes.h>

// OTLP HTTP metrics exporter
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>

namespace
{
// ── Per-metric Observable callbacks ──────────────────────────────────────────
// Each callback receives a void* pointing to the cache statistics callback.

using StatsCallback = SmartMet::Spine::OTelMetricsExporter::CacheStatsCallback;
using Int64Result = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>;

// Report one value per cache, selected from its statistics by the given getter
template <typename Getter>
void observe(opentelemetry::metrics::ObserverResult result, void* state, Getter getter)
{
  // The instruments are int64 gauges, so the result always holds the int64 alternative
  if (!opentelemetry::nostd::holds_alternative<Int64Result>(result))
    return;
  auto& observer = opentelemetry::nostd::get<Int64Result>(result);

  const auto stats = (*static_cast<const StatsCallback*>(state))();
  for (const auto& [name, s] : stats)
    observer->Observe(static_cast<int64_t>(getter(s)),
                      {{"cache.name", opentelemetry::nostd::string_view(name)}});
}

void observeSize(opentelemetry::metrics::ObserverResult result, void* state)
{
  observe(result, state, [](const auto& s) { return s.size; });
}

void observeMaxSize(opentelemetry::metrics::ObserverResult result, void* state)
{
  observe(result, state, [](const auto& s) { return s.maxsize; });
}

void observeHits(opentelemetry::metrics::ObserverResult result, void* state)
{
  observe(result, state, [](const auto& s) { return s.hits; });
}

void observeMisses(opentelemetry::metrics::ObserverResult result, void* state)
{
  observe(result, state, [](const auto& s) { return s.misses; });
}

void observeInserts(opentelemetry::metrics::ObserverResult result, void* state)
{
  observe(result, state, [](const auto& s) { return s.inserts; });
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
        {opentelemetry::semconv::service::kServiceName, itsOptions.service_name},
        {opentelemetry::semconv::service::kServiceVersion, itsOptions.service_version},
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

    auto* ctx_raw = &itsContext->callback;
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
      // Flushing and shutting down collect the metrics one last time, which
      // calls the callbacks. Their context must stay alive until both are done.
      itsProvider->ForceFlush(std::chrono::milliseconds(itsOptions.timeout_ms));
      itsProvider->Shutdown();

      itsSizeGauge = nullptr;
      itsMaxSizeGauge = nullptr;
      itsHitsGauge = nullptr;
      itsMissesGauge = nullptr;
      itsInsertsGauge = nullptr;
      itsProvider.reset();
      itsContext.reset();
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
