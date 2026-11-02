#define BOOST_TEST_MODULE "RequestWatcherTest"

#include "RequestWatcher.h"
#include <boost/chrono.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>
#include <macgyver/Cancellation.h>
#include <macgyver/ThreadPool.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

using namespace SmartMet::Spine;

namespace
{
using Token = Fmi::CancellationToken;

std::shared_ptr<Token> token_with_deadline(std::chrono::milliseconds theDelay)
{
  auto token = std::make_shared<Token>();
  token->setDeadline(Token::Clock::now() + theDelay);
  return token;
}

// Run the task in a thread pool worker and wait for it to finish
void run_in_pool(const std::function<void()>& theTask)
{
  std::atomic<bool> done{false};
  Fmi::ThreadPool::ThreadPool<> pool(1, 10);
  pool.setGracefulShutdown(true);
  pool.start();
  BOOST_REQUIRE(pool.schedule(
      [&]()
      {
        theTask();
        done = true;
      }));
  const auto deadline = boost::chrono::steady_clock::now() + boost::chrono::seconds(10);
  while (!done && boost::chrono::steady_clock::now() < deadline)
    boost::this_thread::sleep_for(boost::chrono::milliseconds(5));
  pool.shutdown();
  BOOST_REQUIRE_MESSAGE(done, "the task did not finish");
}
}  // namespace

// A handler waiting at an interruption point is interrupted at the deadline
BOOST_AUTO_TEST_CASE(deadline_interrupts)
{
  RequestWatcher watcher;
  bool interrupted = false;
  bool stopped = false;
  auto elapsed = std::chrono::milliseconds(0);

  run_in_pool(
      [&]()
      {
        const auto start = std::chrono::steady_clock::now();
        auto watch = watcher.watch(token_with_deadline(std::chrono::milliseconds(100)));
        try
        {
          boost::this_thread::sleep_for(boost::chrono::seconds(5));
        }
        catch (const boost::thread_interrupted&)
        {
          interrupted = true;
        }
        stopped = watch.stop();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
      });

  BOOST_CHECK(interrupted);
  BOOST_CHECK(stopped);
  BOOST_CHECK_LT(elapsed.count(), 2000);
}

// An interruption the handler did not notice must not hit the next request
BOOST_AUTO_TEST_CASE(pending_interruption_is_cleared)
{
  RequestWatcher watcher;
  bool stopped = false;
  bool pending = true;

  run_in_pool(
      [&]()
      {
        auto watch = watcher.watch(token_with_deadline(std::chrono::milliseconds(50)));
        // Busy loop without interruption points until the watcher has interrupted
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!boost::this_thread::interruption_requested() &&
               std::chrono::steady_clock::now() < end)
        {
        }
        stopped = watch.stop();
        pending = boost::this_thread::interruption_requested();
      });

  BOOST_CHECK(stopped);
  BOOST_CHECK(!pending);
}

// A handler finishing in time is not interrupted
BOOST_AUTO_TEST_CASE(in_time)
{
  RequestWatcher watcher;
  bool stopped = true;
  bool pending = true;

  run_in_pool(
      [&]()
      {
        auto watch = watcher.watch(token_with_deadline(std::chrono::milliseconds(200)));
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
        stopped = watch.stop();
        boost::this_thread::sleep_for(boost::chrono::milliseconds(400));
        pending = boost::this_thread::interruption_requested();
      });

  BOOST_CHECK(!stopped);
  BOOST_CHECK(!pending);
}

// A probe noticing a client which has left interrupts too
BOOST_AUTO_TEST_CASE(probe_interrupts)
{
  RequestWatcher watcher;
  bool interrupted = false;

  run_in_pool(
      [&]()
      {
        auto token = std::make_shared<Token>();
        const auto gone = Token::Clock::now() + std::chrono::milliseconds(100);
        token->setProbe(
            [gone]()
            {
              return Token::Clock::now() >= gone ? Token::Reason::Disconnected
                                                 : Token::Reason::None;
            },
            std::chrono::milliseconds(10));
        auto watch = watcher.watch(token);
        try
        {
          boost::this_thread::sleep_for(boost::chrono::seconds(5));
        }
        catch (const boost::thread_interrupted&)
        {
          interrupted = true;
        }
        watch.stop();
      });

  BOOST_CHECK(interrupted);
}

// Nothing is watched outside thread pool workers, or without a deadline or a probe
BOOST_AUTO_TEST_CASE(nothing_to_watch)
{
  RequestWatcher watcher;
  auto outside = watcher.watch(token_with_deadline(std::chrono::milliseconds(0)));
  boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  BOOST_CHECK(!outside.stop());

  bool stopped = true;
  run_in_pool(
      [&]()
      {
        auto watch = watcher.watch(std::make_shared<Token>());
        auto none = watcher.watch(nullptr);
        stopped = watch.stop() || none.stop();
      });
  BOOST_CHECK(!stopped);
}
