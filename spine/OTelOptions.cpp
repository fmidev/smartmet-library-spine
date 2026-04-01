#include "OTelOptions.h"
#include <libconfig.h++>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Spine
{

OTelOptions OTelOptions::fromConfig(const libconfig::Config& config)
{
  try
  {
    OTelOptions opts;

    if (!config.exists("opentelemetry"))
      return opts;

    const libconfig::Setting& cfg = config.lookup("opentelemetry");

    cfg.lookupValue("enabled", opts.enabled);
    cfg.lookupValue("endpoint", opts.endpoint);
    cfg.lookupValue("service_name", opts.service_name);
    cfg.lookupValue("service_version", opts.service_version);
    cfg.lookupValue("timeout_ms", opts.timeout_ms);
    cfg.lookupValue("batch", opts.batch);

    int tmp = 0;
    if (cfg.lookupValue("batch_max_queue_size", tmp))
      opts.batch_max_queue_size = static_cast<std::size_t>(tmp);
    cfg.lookupValue("batch_schedule_delay_ms", opts.batch_schedule_delay_ms);
    cfg.lookupValue("metrics_interval_s", opts.metrics_interval_s);

    if (cfg.exists("headers"))
    {
      const libconfig::Setting& hdrs = cfg.lookup("headers");
      for (int i = 0; i < hdrs.getLength(); ++i)
      {
        std::string key = hdrs[i].getName();
        std::string val = hdrs[i];
        opts.headers[key] = val;
      }
    }

    return opts;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse opentelemetry configuration");
  }
}

}  // namespace Spine
}  // namespace SmartMet
