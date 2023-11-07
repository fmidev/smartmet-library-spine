#pragma once

#include "HTTP.h"
#include "Thread.h"
#include <macgyver/DateTime.h>
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

class ActiveRequests
{
 public:
  // Storage for the request information
  struct Info
  {
    HTTP::Request request;
    Fmi::DateTime time;
  };

  using Requests = std::map<std::size_t, Info>;
  ActiveRequests() = default;
  ActiveRequests(const ActiveRequests& other) = delete;
  ActiveRequests(ActiveRequests&& other) = delete;
  ActiveRequests& operator=(ActiveRequests&& other) = delete;
  ActiveRequests& operator=(const ActiveRequests& other) = delete;

  std::size_t insert(const HTTP::Request& theRequest);
  void remove(std::size_t theKey);
  Requests requests() const;
  std::size_t size() const;
  std::size_t counter() const;  // how many requests have completed

 private:
  mutable MutexType itsMutex;
  std::atomic<std::size_t> itsStartedCounter{0};   // number of started requests
  std::atomic<std::size_t> itsFinishedCounter{0};  // number of completed requests
  Requests itsRequests;
};
}  // namespace Spine
}  // namespace SmartMet
