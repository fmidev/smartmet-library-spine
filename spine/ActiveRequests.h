#pragma once

#include "HTTP.h"
#include "Thread.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/noncopyable.hpp>
#include <map>
#include <string>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Track active requests.
 *
 * Use case: 1) User inserts URI and time, obtains key for the request.
 *           2) User removes returned key when request is complete.
 *
 * Use case: User requests information on all active requests
 */
// ----------------------------------------------------------------------

class ActiveRequests : private boost::noncopyable
{
 public:
  // Storage for the request information
  struct Info
  {
    HTTP::Request request;
    boost::posix_time::ptime time;
  };

  using Requests = std::map<std::size_t, Info>;

 public:
  std::size_t insert(const HTTP::Request& theRequest);
  void remove(std::size_t theKey);
  Requests requests() const;
  std::size_t size() const;

 private:
  mutable MutexType itsMutex;
  std::size_t itsCounter = 0;
  Requests itsRequests;
};
}  // namespace Spine
}  // namespace SmartMet
