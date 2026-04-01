#pragma once

#include "OTelOptions.h"
#include <functional>
#include <macgyver/CacheStats.h>

#ifdef SMARTMET_SPINE_OPENTELEMETRY
namespace opentelemetry { namespace sdk { namespace metrics { class MeterProvider; } } }
namespace opentelemetry { namespace metrics { class ObservableInstrument; } }
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
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsSizeGauge;
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsMaxSizeGauge;
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsHitsGauge;
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsMissesGauge;
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument> itsInsertsGauge;
#endif
};

}  // namespace Spine
}  // namespace SmartMet
