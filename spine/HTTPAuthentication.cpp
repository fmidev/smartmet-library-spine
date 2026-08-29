#include "HTTPAuthentication.h"
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <macgyver/Base64.h>
#include <macgyver/Exception.h>
#include <macgyver/TypeName.h>

#include <algorithm>
#include <cstddef>

namespace ba = boost::algorithm;

namespace SmartMet
{
namespace Spine
{
namespace HTTP
{
namespace
{
// Case-sensitive, constant-time string comparison. The Base64 credential digests are
// case-sensitive, so the previous case-insensitive compare both weakened the check and
// short-circuited on the first differing byte, leaking how much of the digest matched
// through timing. This compares over the whole length without an early return.
bool constantTimeEquals(const std::string& a, const std::string& b)
{
  const std::size_t na = a.size();
  const std::size_t nb = b.size();
  const std::size_t n = std::max(na, nb);
  unsigned char diff = static_cast<unsigned char>((na ^ nb) != 0 ? 1 : 0);
  for (std::size_t i = 0; i < n; ++i)
  {
    const unsigned char ca = (i < na) ? static_cast<unsigned char>(a[i]) : 0;
    const unsigned char cb = (i < nb) ? static_cast<unsigned char>(b[i]) : 0;
    diff |= static_cast<unsigned char>(ca ^ cb);
  }
  return diff == 0;
}
}  // namespace
Authentication::Authentication(bool denyByDefault) : denyByDefault(denyByDefault) {}

Authentication::~Authentication() = default;

void Authentication::addUser(const std::string& name,
                             const std::string& password,
                             unsigned groupMask)
{
  userMap[name] = std::make_pair(password, groupMask);
}

bool Authentication::removeUser(const std::string& name)
{
  return userMap.erase(name) > 0;
}

void Authentication::clearUsers()
{
  userMap.clear();
}

bool Authentication::authenticateRequest(const Request& request, Response& response)
{
  try
  {
    auto credentials = request.getHeader("Authorization");

    if (credentials)
    {
      std::vector<std::string> splitHeader;
      ba::split(splitHeader, *credentials, ba::is_any_of(" "), ba::token_compress_on);

      if (splitHeader.size() < 2)
      {
        // Corrupt Authorization header - 400 Bad request
        badRequestResponse(response);
        return false;
      }

      if (ba::iequals(splitHeader.at(0), "basic"))
      {
        auto givenDigest = splitHeader.at(1);
        unsigned groupMask = getAuthenticationGroup(request);
        for (auto& item : userMap)
        {
          if ((item.second.second & groupMask) != 0)
          {
            auto trueDigest = Fmi::Base64::encode(item.first + ":" + item.second.first);
            if (constantTimeEquals(trueDigest, givenDigest))
            {
              return true;
            }
          }
        }
        unauthorizedResponse(response);
        return false;
      }

      // Not supported or invalid authentication type
      badRequestResponse(response);
      return false;
    }

    // Ask whether authentication is required for request if not provided there
    if (!isAuthenticationRequired(request))
      return true;

    printf("%s: No credentials available\n", METHOD_NAME.c_str());

    if (userMap.empty() and not denyByDefault)
      return true;

    unauthorizedResponse(response);
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Authentication::isAuthenticationRequired(const Request& request) const
{
  (void)request;
  return true;
}

unsigned Authentication::getAuthenticationGroup(const Request& request) const
{
  (void)request;
  return static_cast<unsigned>(-1);
}

std::string Authentication::getRealm() const
{
  return "SmartMet server";
}

void Authentication::unauthorizedResponse(Response& response)
{
  response.setStatus(Status::unauthorized);
  response.setHeader("WWW-Authenticate", "Basic realm=\"" + getRealm() + "\"");
  response.setHeader("Content-Type", "text/html; charset=UTF-8");
  const std::string content = "<html><body><h1>401 Unauthorized </h1></body></html>\n";
  response.setContent(content);
}

void Authentication::badRequestResponse(Response& response)
{
  response.setStatus(Status::bad_request);
  response.setHeader("WWW-Authenticate", "Basic realm=\"" + getRealm() + "\"");
  response.setHeader("Content-Type", "text/html; charset=UTF-8");
  const std::string content = "<html><body><h1>400 Bad request </h1></body></html>\n";
  response.setContent(content);
}

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet
