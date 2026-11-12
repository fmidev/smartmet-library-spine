#pragma once

#include "OTelOptions.h"
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <memory>
#include <string>

namespace SmartMet::Spine
{
class LoggedRequest;
}  // namespace SmartMet::Spine

#ifdef SMARTMET_SPINE_OPENTELEMETRY

// Forward-declare the SDK types we hold by shared_ptr to avoid leaking
// heavy opentelemetry headers into every translation unit that includes this.
// The SDK types live in a versioned inline namespace (opentelemetry::v1), so
// they must be declared through its namespace macros.
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
 * OTelLogger — sibling to AccessLogger.
 *
 * Each call to log() creates a finished OTel span that represents one HTTP
 * request.  The span uses retrospective start/end timestamps taken directly
 * from LoggedRequest so that spans land at the correct wall-clock position
 * in the trace backend even though they are exported after the fact.
 *
 * Semantic attributes follow OTel HTTP Semantic Conventions v1.23+:
 *   http.request.method, url.path, http.response.status_code,
 *   client.address, http.response.body.size, http.route
 *
 * All loggers share one tracer provider, so the server has a single span
 * exporter and batch thread however many handlers it has. The last logger to
 * stop flushes the pending spans and shuts the exporter down.
 *
 * The class is a no-op when compiled without SMARTMET_SPINE_OPENTELEMETRY,
 * or when the "traces" setting is off.
 */
class OTelLogger
{
 public:
  OTelLogger(std::string resource, OTelOptions options);
  ~OTelLogger();

  OTelLogger(const OTelLogger&) = delete;
  OTelLogger& operator=(const OTelLogger&) = delete;
  OTelLogger(OTelLogger&&) = delete;
  OTelLogger& operator=(OTelLogger&&) = delete;

  void start();
  void stop();
  void log(const LoggedRequest& request);
  void flush();

 private:
  std::string itsResource;
  OTelOptions itsOptions;
  bool itsIsRunning = false;

#ifdef SMARTMET_SPINE_OPENTELEMETRY
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> itsTracer;
  std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> itsProvider;
#endif
};

}  // namespace Spine
}  // namespace SmartMet
