#include "OTelLogger.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

#ifdef SMARTMET_SPINE_OPENTELEMETRY

// OTel API
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span_startoptions.h>
#include <opentelemetry/common/timestamp.h>

// OTel SDK — tracer provider + processors
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/semconv/service_attributes.h>

// OTLP HTTP exporter
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>

#include <chrono>
#include <memory>
#include <mutex>

namespace
{
// Convert a UTC Fmi::DateTime to a system_clock time_point so that we can hand
// an absolute wall-clock timestamp to OTel.
std::chrono::system_clock::time_point fmiToTimePoint(const Fmi::DateTime& dt)
{
  const auto us = (dt - Fmi::DateTime::epoch).total_microseconds();
  return std::chrono::system_clock::time_point(std::chrono::microseconds(us));
}

// OTel measures span durations on the steady clock, and the end time of a
// span can only be given as a steady timestamp. Map a past wall-clock time
// to the steady clock by its distance from the current time.
std::chrono::steady_clock::time_point toSteady(std::chrono::system_clock::time_point tp)
{
  const auto age = std::chrono::system_clock::now() - tp;
  return std::chrono::steady_clock::now() -
         std::chrono::duration_cast<std::chrono::steady_clock::duration>(age);
}

// All handlers share one tracer provider, so the server has a single span
// exporter and batch thread instead of one per handler. The provider lives as
// long as some OTelLogger holds it, and the last one to stop flushes it.
std::mutex gProviderMutex;
std::weak_ptr<opentelemetry::sdk::trace::TracerProvider> gProvider;

std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> createProvider(
    const SmartMet::Spine::OTelOptions& options)
{
  namespace otlp = opentelemetry::exporter::otlp;
  namespace sdktrace = opentelemetry::sdk::trace;
  namespace res = opentelemetry::sdk::resource;

  // ── Exporter ─────────────────────────────────────────────────────────────
  otlp::OtlpHttpExporterOptions exp_opts;
  exp_opts.url = options.endpoint + "/v1/traces";
  exp_opts.timeout = std::chrono::milliseconds(options.timeout_ms);
  for (const auto& [k, v] : options.headers)
    exp_opts.http_headers.insert({k, v});

  auto exporter = otlp::OtlpHttpExporterFactory::Create(exp_opts);

  // ── Span processor ───────────────────────────────────────────────────────
  std::unique_ptr<sdktrace::SpanProcessor> processor;
  if (options.batch)
  {
    sdktrace::BatchSpanProcessorOptions batch_opts;
    batch_opts.max_queue_size = options.batch_max_queue_size;
    batch_opts.schedule_delay_millis = std::chrono::milliseconds(options.batch_schedule_delay_ms);
    processor = sdktrace::BatchSpanProcessorFactory::Create(std::move(exporter), batch_opts);
  }
  else
  {
    processor = sdktrace::SimpleSpanProcessorFactory::Create(std::move(exporter));
  }

  // ── Resource (service identity; the handler is the http.route span attribute) ──
  res::ResourceAttributes res_attrs = {
      {opentelemetry::semconv::service::kServiceName, options.service_name},
      {opentelemetry::semconv::service::kServiceVersion, options.service_version},
  };
  auto resource = res::Resource::Create(res_attrs);

  return sdktrace::TracerProviderFactory::Create(std::move(processor), resource);
}

std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> acquireProvider(
    const SmartMet::Spine::OTelOptions& options)
{
  std::lock_guard<std::mutex> lock(gProviderMutex);
  auto provider = gProvider.lock();
  if (!provider)
  {
    provider = createProvider(options);
    gProvider = provider;
  }
  return provider;
}

// Drop one reference to the shared provider. The last holder flushes the
// pending spans and shuts the exporter down.
void releaseProvider(std::shared_ptr<opentelemetry::sdk::trace::TracerProvider>& provider,
                     int timeout_ms)
{
  std::lock_guard<std::mutex> lock(gProviderMutex);
  if (provider.use_count() == 1)
  {
    provider->ForceFlush(std::chrono::milliseconds(timeout_ms));
    provider->Shutdown();
  }
  provider.reset();
}
}  // namespace

#endif  // SMARTMET_SPINE_OPENTELEMETRY

namespace SmartMet
{
namespace Spine
{

OTelLogger::OTelLogger(std::string resource, OTelOptions options)
    : itsResource(std::move(resource)), itsOptions(std::move(options))
{
}

OTelLogger::~OTelLogger()
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

void OTelLogger::start()
{
  try
  {
#ifdef SMARTMET_SPINE_OPENTELEMETRY
    if (!itsOptions.enabled || !itsOptions.traces)
      return;

    itsProvider = acquireProvider(itsOptions);
    itsTracer = itsProvider->GetTracer(itsOptions.service_name, itsOptions.service_version);
    itsIsRunning = true;
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OTelLogger::start failed");
  }
}

void OTelLogger::stop()
{
  try
  {
#ifdef SMARTMET_SPINE_OPENTELEMETRY
    itsTracer = nullptr;
    if (itsProvider)
      releaseProvider(itsProvider, itsOptions.timeout_ms);
#endif
    itsIsRunning = false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OTelLogger::stop failed");
  }
}

void OTelLogger::log(const LoggedRequest& request)
{
  try
  {
#ifdef SMARTMET_SPINE_OPENTELEMETRY
    if (!itsIsRunning || !itsTracer)
      return;

    namespace trace = opentelemetry::trace;

    // ── Start time ───────────────────────────────────────────────────────────
    const auto start_time = fmiToTimePoint(request.getRequestStartTime());
    const auto end_time = fmiToTimePoint(request.getRequestEndTime());

    trace::StartSpanOptions start_opts;
    start_opts.start_system_time = opentelemetry::common::SystemTimestamp(start_time);
    start_opts.start_steady_time = opentelemetry::common::SteadyTimestamp(toSteady(start_time));

    // ── Create span ──────────────────────────────────────────────────────────
    // Name follows OTel HTTP semantic conventions: "METHOD /route"
    const std::string span_name = request.getMethod() + " " + itsResource;

    auto span = itsTracer->StartSpan(
        span_name,
        {
            {"http.request.method",         request.getMethod()},
            {"url.path",                    request.getRequestString()},
            {"client.address",              request.getIP()},
            {"http.route",                  itsResource},
            {"network.protocol.version",    request.getVersion()},
        },
        start_opts);

    // ── Response attributes (set after the fact) ─────────────────────────────
    const int status = std::stoi(request.getStatus());
    span->SetAttribute("http.response.status_code",
                       static_cast<int64_t>(status));
    if (request.getContentLength() > 0)
      span->SetAttribute("http.response.body.size",
                         static_cast<int64_t>(request.getContentLength()));
    if (!request.getETag().empty() && request.getETag() != "-")
      span->SetAttribute("http.response.header.etag", request.getETag());

    // OTel status: Error for 5xx, Ok otherwise
    if (status >= 500)
      span->SetStatus(trace::StatusCode::kError, request.getStatus());
    else
      span->SetStatus(trace::StatusCode::kOk);

    // ── End time ─────────────────────────────────────────────────────────────
    trace::EndSpanOptions end_opts;
    end_opts.end_steady_time = opentelemetry::common::SteadyTimestamp(toSteady(end_time));
    span->End(end_opts);
#else
    (void)request;
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OTelLogger::log failed");
  }
}

void OTelLogger::flush()
{
  // Nothing to do: the batch processor exports on its own schedule, the simple
  // processor exports each span when it ends, and the last logger to stop
  // flushes the shared provider. Forcing an export here would make every
  // handler's access log flush export the spans of all handlers.
}

}  // namespace Spine
}  // namespace SmartMet
