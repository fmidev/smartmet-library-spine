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

}  // namespace Spine
}  // namespace SmartMet
