#include "FmiApiKey.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Spine
{
namespace FmiApiKey
{
// ----------------------------------------------------------------------
/*!
 * \brief Get request apikey
 *
 * 	  1) from request header 'fmi-apikey'
 *
 *	or
 * 	  2) from request parameter 'fmi-apikey'
 *
 *	or if checkAccessToken is set (wfs and wcs)
 * 	  3) from request parameter 'access-token'
 */
// ----------------------------------------------------------------------

std::optional<std::string> getFmiApiKey(const HTTP::Request& theRequest, bool checkAccessToken)
{
  try
  {
    auto apikey = theRequest.getHeader("fmi-apikey");

    if (!apikey)
    {
      apikey = theRequest.getParameter("fmi-apikey");

      if (!apikey && checkAccessToken)
        apikey = theRequest.getParameter("access-token");
    }

    return apikey;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace FmiApiKey
}  // namespace Spine
}  // namespace SmartMet
