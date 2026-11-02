#include "RequestWatcher.h"
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/thread.hpp>
#include <macgyver/Cancellation.h>
#include <macgyver/Exception.h>
#include <macgyver/ThreadName.h>
#include <macgyver/ThreadPool.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace SmartMet
{
namespace Spine
{
namespace
{
// How often the probe of a token is polled, the server polls it as often
const auto probe_interval = std::chrono::seconds(1);
}  // namespace

struct RequestWatcher::Watch::State : std::enable_shared_from_this<State>
{
  State(boost::asio::io_context& theContext,
        std::shared_ptr<Fmi::CancellationToken> theToken,
        boost::thread* theThread)
      : timer(theContext), token(std::move(theToken)), thread(theThread)
  {
  }

  // Wait for the deadline, or for the next poll of the probe. Called with the mutex locked.
  void wait()
  {
    const auto deadline = token->deadline();
    auto next = Fmi::CancellationToken::Clock::time_point::max();
    if (token->hasProbe())
      next = Fmi::CancellationToken::Clock::now() + probe_interval;
    if (deadline)
      next = std::min(next, *deadline);

    timer.expires_at(next);
    timer.async_wait([self = shared_from_this()](const boost::system::error_code& err)
                     { self->poll(err); });
  }

  void poll(const boost::system::error_code& err)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (err || stopped)
      return;

    if (!token->cancelled())
    {
      wait();
      return;
    }

    interrupted = true;
    thread->interrupt();
  }

  boost::asio::steady_timer timer;
  std::shared_ptr<Fmi::CancellationToken> token;
  boost::thread* thread;

  std::mutex mutex;
  bool stopped = false;
  bool interrupted = false;
};

class RequestWatcher::Impl
{
 public:
  Impl() : work(boost::asio::make_work_guard(context)), thread([this]() { run(); }) {}

  ~Impl()
  {
    work.reset();
    context.stop();
    if (thread.joinable())
      thread.join();
  }

  Impl(const Impl& other) = delete;
  Impl(Impl&& other) = delete;
  Impl& operator=(const Impl& other) = delete;
  Impl& operator=(Impl&& other) = delete;

  boost::asio::io_context context;

 private:
  void run()
  {
    Fmi::set_thread_name("req-watch");
    while (!context.stopped())
    {
      try
      {
        context.run();
      }
      catch (...)
      {
        std::cerr << Fmi::Exception::Trace(BCP, "Request watcher failed") << std::endl;
      }
    }
  }

  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work;
  std::thread thread;
};

RequestWatcher::RequestWatcher() : impl(std::make_unique<Impl>()) {}

RequestWatcher::~RequestWatcher() = default;

RequestWatcher::Watch RequestWatcher::watch(const std::shared_ptr<Fmi::CancellationToken>& theToken)
{
  try
  {
    auto* thread = Fmi::ThreadPool::current_worker_thread();
    if (!theToken || !thread || (!theToken->deadline() && !theToken->hasProbe()))
      return {};

    auto state = std::make_shared<Watch::State>(impl->context, theToken, thread);
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->wait();
    }
    return Watch(std::move(state));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestWatcher::Watch::Watch(std::shared_ptr<State> theState) : itsState(std::move(theState)) {}

RequestWatcher::Watch::~Watch()
{
  try
  {
    stop();
  }
  catch (...)
  {
    std::cerr << Fmi::Exception::Trace(BCP, "Failed to stop watching a request") << std::endl;
  }
}

bool RequestWatcher::Watch::stop()
{
  if (!itsState)
    return itsInterrupted;

  {
    std::lock_guard<std::mutex> lock(itsState->mutex);
    itsState->stopped = true;
    itsState->timer.cancel();
    itsInterrupted = itsState->interrupted;
  }
  itsState.reset();

  if (itsInterrupted && boost::this_thread::interruption_requested())
  {
    try
    {
      boost::this_thread::interruption_point();
    }
    catch (const boost::thread_interrupted&)
    {
    }
  }
  return itsInterrupted;
}

}  // namespace Spine
}  // namespace SmartMet
