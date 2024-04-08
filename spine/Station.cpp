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

std::size_t Station::hash() const
{
  // At FMI fmisid is an unique identifier assigned for all stations
  return fmisid;
}

const std::string& Station::station_formal_name(const std::string& language) const
{
  if (language == "fi")
    return formal_name_fi;
  if (language == "sv" && !formal_name_sv.empty())
    return formal_name_sv;
  if (language == "en" && !formal_name_en.empty())
    return formal_name_en;

  // If sv/en are empty return fi-version, which should never be empty
  return formal_name_fi;
}

}  // namespace Spine
}  // namespace SmartMet
