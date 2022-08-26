#include "Exceptions.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <libconfig.h++>

namespace SmartMet
{
namespace Spine
{
namespace Exceptions
{
// Generic exception handler for typical configuration problems
void handle(const std::string& name)
{
  try
  {
    throw;
  }
  catch (const libconfig::SettingNotFoundException& e)
  {
    throw Fmi::Exception(BCP, "Configuration file setting not found")
        .addParameter("Component", name)
        .addParameter("Setting path", e.getPath());
  }
  catch (const libconfig::SettingTypeException& e)
  {
    throw Fmi::Exception(BCP, "Incorrect configuration file setting type")
        .addParameter("Component", name)
        .addParameter("Setting path", e.getPath());
  }
  catch (const libconfig::ParseException& e)
  {
    throw Fmi::Exception::Trace(BCP, "Configuration file syntax error")
        .addParameter("Component", name)
        .addParameter("Line", Fmi::to_string(e.getLine()));
  }
  catch (const libconfig::ConfigException& e)
  {
    throw Fmi::Exception::Trace(BCP, "Configuration file error")
        .addParameter("Component", name)
        .addParameter("Message", e.what());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Error").addParameter("Component", name);
  }
}
}  // namespace Exceptions
}  // namespace Spine
}  // namespace SmartMet
