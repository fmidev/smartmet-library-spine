#pragma once

#include <map>
#include <string>

// Forward declaration to avoid pulling in <libconfig.h++> here
namespace libconfig { class Config; }

namespace SmartMet
{
namespace Spine
{

/**
 * Configuration for OpenTelemetry tracing export.
 *
 * Populated from the top-level "opentelemetry" group in smartmet.conf:
 *
 *   opentelemetry:
 *   {
 *     enabled        = true;
 *     endpoint       = "http://localhost:4318";   # OTLP/HTTP base URL
 *     service_name   = "smartmet-server";
 *     service_version = "1.0";
 *     timeout_ms     = 10000;
 *     batch          = true;                      # batch vs simple processor
 *     batch_max_queue_size    = 2048;
 *     batch_schedule_delay_ms = 5000;
 *     headers = { Authorization = "Bearer <token>"; };
 *   };
 */
struct OTelOptions
{
  bool enabled = false;

  // OTLP HTTP endpoint base URL (without /v1/traces suffix)
  std::string endpoint = "http://localhost:4318";

  std::string service_name = "smartmet-server";
  std::string service_version;

  int timeout_ms = 10000;

  // Extra HTTP headers sent with every OTLP export (e.g. auth tokens)
  std::map<std::string, std::string> headers;

  // Use BatchSpanProcessor (true) or SimpleSpanProcessor (false)
  bool batch = true;
  std::size_t batch_max_queue_size = 2048;
  int batch_schedule_delay_ms = 5000;

  // How often to push cache-statistics metrics (seconds).  0 = disabled.
  int metrics_interval_s = 60;

  /**
   * Parse OTelOptions from the "opentelemetry" group in a libconfig::Config.
   * Returns default-constructed (disabled) options if the group is absent.
   */
  static OTelOptions fromConfig(const libconfig::Config& config);
};

}  // namespace Spine
}  // namespace SmartMet
