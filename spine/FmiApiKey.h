// ======================================================================
/*!
 * \brief FmiApiKey tools
 */
//======================================================================

#pragma once
#include <optional>
#include <string>

namespace SmartMet::Spine::HTTP
{
class Request;
}  // namespace SmartMet::Spine::HTTP

namespace SmartMet
{
namespace Spine
{
namespace FmiApiKey
{
// Get request apikey from header or url
std::optional<std::string> getFmiApiKey(const HTTP::Request& theRequest,
                                          bool checkAccessToken = false);

// Should the API key be returned in responses?
bool shouldReturnApiKey(const HTTP::Request& theRequest);

}  // namespace FmiApiKey
}  // namespace Spine
}  // namespace SmartMet
