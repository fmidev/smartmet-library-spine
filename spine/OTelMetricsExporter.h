#pragma once

#include "OTelOptions.h"
#include <macgyver/CacheStats.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <functional>
#include <memory>

#ifdef SMARTMET_SPINE_OPENTELEMETRY
// The SDK types live in a versioned inline namespace (opentelemetry::v1), so
// they must be forward-declared through its namespace macros.
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/version.h>

OPENTELEMETRY_BEGIN_NAMESPACE
OPENTELEMETRY_END_NAMESPACE
#endif

namespace SmartMet
{
namespace Spine
{

/**
 * OTelMetricsExporter — periodically exports cache-statistics gauges via OTel Metrics.
 *
 * The caller supplies a callback that returns the current Fmi::Cache::CacheStatistics map.
 * The PeriodicExportingMetricReader (part of the SDK) calls back into the registered
 * Observable instruments on each export cycle.
 *
 * Published metrics (all use attribute "cache.name" = <cache key>):
 *
 *   smartmet.cache.size        – current number of entries
 *   smartmet.cache.max_size    – configured capacity
 *   smartmet.cache.hits        – cumulative hits since start
 *   smartmet.cache.misses      – cumulative misses since start
 *   smartmet.cache.inserts     – cumulative inserts since start
 *
 * The export interval is taken from OTelOptions::metrics_interval_s (default 60 s).
 * Set metrics_interval_s = 0 to disable metrics export entirely.
 *
 * This class is a no-op when compiled without SMARTMET_SPINE_OPENTELEMETRY.
 */
class OTelMetricsExporter
{
 public:
  using CacheStatsCallback = std::function<Fmi::Cache::CacheStatistics()>;

  OTelMetricsExporter(OTelOptions options, CacheStatsCallback callback);
  ~OTelMetricsExporter();

  OTelMetricsExporter(const OTelMetricsExporter&) = delete;
  OTelMetricsExporter& operator=(const OTelMetricsExporter&) = delete;
  OTelMetricsExporter(OTelMetricsExporter&&) = delete;
  OTelMetricsExporter& operator=(OTelMetricsExporter&&) = delete;

  void start();
  void stop();

 private:
  OTelOptions itsOptions;
  CacheStatsCallback itsCallback;
  bool itsIsRunning = false;

#ifdef SMARTMET_SPINE_OPENTELEMETRY
  // Context shared with the Observable callbacks (kept alive until stop())
  struct ObserveContext
  {
    CacheStatsCallback callback;
  };
  std::shared_ptr<ObserveContext> itsContext;

  std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> itsProvider;

  // Observable instruments must be kept alive; destruction removes the callback.
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsSizeGauge;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsMaxSizeGauge;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsHitsGauge;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsMissesGauge;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsInsertsGauge;
#endif
};

}  // namespace Spine
}  // namespace SmartMet
