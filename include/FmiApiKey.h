// ======================================================================
/*!
 * \brief FmiApiKey tools
 */
//======================================================================

#pragma once
#include "HTTP.h"
#include "Exception.h"
#include <boost/optional.hpp>
#include <string>

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

boost::optional<std::string> getFmiApiKey(const HTTP::Request& theRequest,
                                          bool checkAccessToken = false)
{
  try
  {
    auto apikey = theRequest.getHeader("fmi-apikey");

    if (!apikey)
    {
      apikey = theRequest.getParameter("fmi-apikey");

      if (!apikey && checkAccessToken)
        apikey = theRequest.getHeader("access-token");
    }

    return apikey;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace FmiApiKey
}  // namespace Spine
}  // namespace SmartMet
