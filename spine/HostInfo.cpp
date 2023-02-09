#include "HostInfo.h"
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>

namespace SmartMet
{
namespace Spine
{
namespace HostInfo
{
// Find host name for the given IP. Returns empty string on resolve error

std::string getHostName(const std::string& theIP)
{
  struct sockaddr_in sa;
  char node[NI_MAXHOST];

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;

  inet_pton(AF_INET, theIP.c_str(), &sa.sin_addr);

  int res =
      getnameinfo((struct sockaddr*)&sa, sizeof(sa), node, sizeof(node), NULL, 0, NI_NAMEREQD);

  if (res)
    return {};

  return node;
}

}  // namespace HostInfo
}  // namespace Spine
}  // namespace SmartMet
