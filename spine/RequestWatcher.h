#pragma once

#include <memory>

namespace Fmi
{
class CancellationToken;
}

namespace SmartMet
{
namespace Spine
{
// Interrupts the thread handling a request with boost::thread::interrupt() once the request
// has been cancelled (Fmi::CancellationToken): its time limit has passed or its client has
// left. The handler then stops at the next boost interruption point, also where it does not
// call Fmi::check_cancelled(). One thread watches all the requests.
class RequestWatcher
{
 public:
  class Watch;

  RequestWatcher();
  ~RequestWatcher();
  RequestWatcher(const RequestWatcher& other) = delete;
  RequestWatcher(RequestWatcher&& other) = delete;
  RequestWatcher& operator=(const RequestWatcher& other) = delete;
  RequestWatcher& operator=(RequestWatcher&& other) = delete;

  // Start watching the request handled by the calling thread. Nothing is watched if the
  // thread is not a thread pool worker, or if the token has neither a deadline nor a probe.
  Watch watch(const std::shared_ptr<Fmi::CancellationToken>& theToken);

 private:
  class Impl;
  std::unique_ptr<Impl> impl;
};

// Watching one request, stopped at the latest when destroyed
class RequestWatcher::Watch
{
 public:
  Watch() = default;
  ~Watch();
  Watch(const Watch& other) = delete;
  Watch(Watch&& other) noexcept = default;
  Watch& operator=(const Watch& other) = delete;
  Watch& operator=(Watch&& other) = delete;

  // Stop watching. Returns true if the thread has been interrupted. An interruption the
  // thread has not yet noticed is then cleared, so that it cannot hit the next request
  // handled by the thread. Must be called by the watched thread.
  bool stop();

 private:
  friend class RequestWatcher;
  struct State;
  explicit Watch(std::shared_ptr<State> theState);

  std::shared_ptr<State> itsState;
  bool itsInterrupted = false;
};

}  // namespace Spine
}  // namespace SmartMet
