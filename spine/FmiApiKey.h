// ======================================================================
/*!
 * \brief FmiApiKey tools
 */
//======================================================================

#pragma once
#include "HTTP.h"
#include <boost/optional.hpp>
#include <string>

namespace SmartMet
{
namespace Spine
{
namespace FmiApiKey
{
// Get request apikey from header or url
boost::optional<std::string> getFmiApiKey(const HTTP::Request& theRequest,
                                          bool checkAccessToken = false);

}  // namespace FmiApiKey
}  // namespace Spine
}  // namespace SmartMet
