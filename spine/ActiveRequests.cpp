#include "ActiveRequests.h"
#include "FmiApiKey.h"
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Add a new active request
 */
// ----------------------------------------------------------------------

std::size_t ActiveRequests::insert(const HTTP::Request& theRequest)
{
  WriteLock lock(itsMutex);
  auto key = ++itsStartedCounter;
  Info info{theRequest, Fmi::MicrosecClock::universal_time()};
  itsRequests.insert(Requests::value_type{key, info});
  return key;
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove an active request
 */
// ----------------------------------------------------------------------

void ActiveRequests::remove(std::size_t theKey)
{
  WriteLock lock(itsMutex);
  auto pos = itsRequests.find(theKey);
  if (pos != itsRequests.end())
    itsRequests.erase(pos);
  ++itsFinishedCounter;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return information on active requests
 */
// ----------------------------------------------------------------------

ActiveRequests::Requests ActiveRequests::requests() const
{
  WriteLock lock(itsMutex);
  auto ret = itsRequests;
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the number of active requests
 */
// ----------------------------------------------------------------------

std::size_t ActiveRequests::size() const
{
  WriteLock lock(itsMutex);
  return itsRequests.size();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return number of requests completed
 */
// ----------------------------------------------------------------------

std::size_t ActiveRequests::counter() const
{
  return itsFinishedCounter;
}

// ----------------------------------------------------------------------
/*!
 * \brief Print the active requests
 */
// ----------------------------------------------------------------------

std::ostream& operator << (std::ostream& os, const ActiveRequests::Requests& requests)
{
  if (requests.empty())
  {
    os << "No active requests" << std::endl;
    return os;
  }

  os << "-------------------------" << '\n';
  os << "#### Active requests ####" << '\n';
  os << "-------------------------" << '\n';

  int row = 0;
  auto now = Fmi::MicrosecClock::universal_time();

  for (const auto& id_info : requests)
  {
    const auto id = id_info.first;
    const auto& time = id_info.second.time;
    const auto& req = id_info.second.request;
    const std::string url = req.getURI();
    const std::string decoded_url = HTTP::urldecode(url);
    const std::string client_ip = req.getClientIP();

    auto duration = now - time;

    const bool check_access_token = false;
    auto apikey = FmiApiKey::getFmiApiKey(req, check_access_token);

    os << " request[" << ++row << "]: " << '\n';
    os << "    id             : " << id << '\n';
    os << "    time           : " << Fmi::to_iso_extended_string(time.time_of_day()) << '\n';
    os << "    duration       : " << Fmi::to_string(duration.total_milliseconds() / 1000.0) << " seconds\n";
    os << "    URL            : " << url << '\n';
    if (decoded_url != url)
      os << "    decoded URL    : " << decoded_url << '\n';
    if (client_ip != "")
      os << "    client_ip      : " << client_ip << '\n';
    if (apikey)
      os << "    apikey         : " << *apikey << '\n';
    os << std::flush;
  }
  return os;
}

}  // namespace Spine
}  // namespace SmartMet
