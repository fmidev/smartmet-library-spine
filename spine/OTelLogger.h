#pragma once

#include "LoggedRequest.h"
#include "OTelOptions.h"
#include <string>

#ifdef SMARTMET_SPINE_OPENTELEMETRY

// Forward-declare the SDK types we hold by shared_ptr to avoid leaking
// heavy opentelemetry headers into every translation unit that includes this.
namespace opentelemetry { namespace trace { class Tracer; } }
namespace opentelemetry { namespace sdk { namespace trace { class TracerProvider; } } }

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
 * The class is a no-op when compiled without SMARTMET_SPINE_OPENTELEMETRY.
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
  std::shared_ptr<opentelemetry::trace::Tracer> itsTracer;
  std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> itsProvider;
#endif
};

}  // namespace Spine
}  // namespace SmartMet
