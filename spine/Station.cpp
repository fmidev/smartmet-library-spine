#include "Station.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Station hash for caching purposes
 */
// ----------------------------------------------------------------------

std::string Station::hash() const
{
  try
  {
    std::ostringstream ret;
    ret << wmo << '|' << lpnn << '|' << fmisid << '|' << rwsid;
    return ret.str();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const std::string& Station::station_formal_name(const std::string& language) const
{
  if (language == "fi")
    return station_formal_name_fi;
  if (language == "sv" && !station_formal_name_sv.empty())
    return station_formal_name_sv;
  if (language == "en" && !station_formal_name_en.empty())
    return station_formal_name_en;

  // If sv/en are empty return fi-version, which should never be empty
  return station_formal_name_fi;
}

}  // namespace Spine
}  // namespace SmartMet
