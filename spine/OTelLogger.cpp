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
#include <opentelemetry/sdk/resource/semantic_conventions.h>

// OTLP HTTP exporter
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <chrono>

namespace
{
// Convert Fmi::DateTime (= boost::posix_time::ptime, UTC) to a system_clock
// time_point so that we can hand an absolute wall-clock timestamp to OTel.
std::chrono::system_clock::time_point fmiToTimePoint(const Fmi::DateTime& dt)
{
  static const Fmi::DateTime kEpoch(boost::gregorian::date(1970, 1, 1));
  const auto us = (dt - kEpoch).total_microseconds();
  return std::chrono::system_clock::time_point(std::chrono::microseconds(us));
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
    if (!itsOptions.enabled)
      return;

    namespace otlp    = opentelemetry::exporter::otlp;
    namespace sdktrace = opentelemetry::sdk::trace;
    namespace res      = opentelemetry::sdk::resource;

    // ── Exporter ─────────────────────────────────────────────────────────────
    otlp::OtlpHttpExporterOptions exp_opts;
    exp_opts.url     = itsOptions.endpoint + "/v1/traces";
    exp_opts.timeout = std::chrono::milliseconds(itsOptions.timeout_ms);
    for (const auto& [k, v] : itsOptions.headers)
      exp_opts.http_headers.insert({k, v});

    auto exporter = otlp::OtlpHttpExporterFactory::Create(exp_opts);

    // ── Span processor ───────────────────────────────────────────────────────
    std::unique_ptr<sdktrace::SpanProcessor> processor;
    if (itsOptions.batch)
    {
      sdktrace::BatchSpanProcessorOptions batch_opts;
      batch_opts.max_queue_size      = itsOptions.batch_max_queue_size;
      batch_opts.schedule_delay_millis =
          std::chrono::milliseconds(itsOptions.batch_schedule_delay_ms);
      processor =
          sdktrace::BatchSpanProcessorFactory::Create(std::move(exporter), batch_opts);
    }
    else
    {
      processor = sdktrace::SimpleSpanProcessorFactory::Create(std::move(exporter));
    }

    // ── Resource (service + handler identity) ────────────────────────────────
    res::ResourceAttributes res_attrs = {
        {res::SemanticConventions::kServiceName,    itsOptions.service_name},
        {res::SemanticConventions::kServiceVersion, itsOptions.service_version},
        {"http.route",                               itsResource},
    };
    auto resource = res::Resource::Create(res_attrs);

    // ── Provider + tracer ────────────────────────────────────────────────────
    itsProvider = sdktrace::TracerProviderFactory::Create(std::move(processor), resource);
    itsTracer   = itsProvider->GetTracer(itsOptions.service_name,
                                         itsOptions.service_version);
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
    if (itsProvider)
    {
      itsProvider->ForceFlush(std::chrono::milliseconds(itsOptions.timeout_ms));
      itsProvider->Shutdown();
      itsProvider.reset();
    }
    itsTracer.reset();
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
    trace::StartSpanOptions start_opts;
    start_opts.start_system_time = opentelemetry::common::SystemTimestamp(
        fmiToTimePoint(request.getRequestStartTime()));

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
    end_opts.end_system_time = opentelemetry::common::SystemTimestamp(
        fmiToTimePoint(request.getRequestEndTime()));
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
  try
  {
#ifdef SMARTMET_SPINE_OPENTELEMETRY
    if (itsProvider)
      itsProvider->ForceFlush(std::chrono::milliseconds(itsOptions.timeout_ms));
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OTelLogger::flush failed");
  }
}

}  // namespace Spine
}  // namespace SmartMet
