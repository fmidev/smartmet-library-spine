#include "Station.h"
#include "Exception.h"

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Default constructor
 */
// ----------------------------------------------------------------------

Station::Station()
    : isFMIStation(false),
      isRoadStation(false),
      isMareographStation(false),
      isBuoyStation(false),
      isSYKEStation(false),
      isForeignStation(false),
      station_elevation(0.0)
{
}

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
