// ======================================================================
/*!
 * \brief Bounded, cached, non-blocking reverse-DNS (PTR) resolution.
 *
 * See HostInfo.h for the rationale. The implementation is a small singleton
 * resolver consisting of:
 *
 *   - an LRU cache (IP -> host name) with separate positive/negative TTLs,
 *   - a bounded, deduplicated work queue,
 *   - one or more background worker threads that perform the actual blocking
 *     getnameinfo() call off the request thread and populate the cache.
 *
 * getHostName() consults the cache first (instant), and on a miss enqueues the
 * IP and waits at most Options::timeout_ms for the worker to produce a result.
 * On timeout it returns "" immediately while the worker keeps going and caches
 * the result for the next request from the same IP. A slow or missing PTR can
 * therefore never delay request handling by more than the configured timeout,
 * and repeat / known-bad IPs cost essentially nothing.
 */
// ======================================================================

#include "HostInfo.h"

#include <macgyver/Cache.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace SmartMet
{
namespace Spine
{
namespace HostInfo
{
namespace
{
using SteadyClock = std::chrono::steady_clock;

// Upper bound on simultaneously pending (queued + in-flight) lookups. Caps memory
// and prevents unbounded growth when the resolver is stuck during a DNS outage:
// further cache misses simply log the IP only instead of queuing.
constexpr std::size_t kMaxInFlight = 1000;

// One cache entry: the resolved name ("" means "no PTR record") plus its expiry.
struct CachedHost
{
  std::string name;
  SteadyClock::time_point expires;
};

// Shared state for callers waiting on an in-flight lookup. Several concurrent
// requests for the same IP share one future.
struct Pending
{
  std::promise<std::string> promise;
  std::shared_future<std::string> future{promise.get_future().share()};
};

// Perform the actual blocking reverse lookup. Returns "" if the address cannot
// be parsed or has no PTR record. Runs only on a worker thread, never on the
// request thread, so the lack of a per-call timeout here is harmless.
std::string blockingLookup(const std::string& theIP)
{
  struct sockaddr_in sa;
  char node[NI_MAXHOST];

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;

  // Only IPv4 client addresses are resolved, matching the historical behaviour.
  // An unparseable (e.g. IPv6) address degrades to "no host name".
  if (inet_pton(AF_INET, theIP.c_str(), &sa.sin_addr) != 1)
    return {};

  int res =
      getnameinfo((struct sockaddr*)&sa, sizeof(sa), node, sizeof(node), nullptr, 0, NI_NAMEREQD);

  if (res != 0)
    return {};

  return node;
}

class Resolver
{
 public:
  static Resolver& instance()
  {
    // Intentionally leaked: a process-lifetime singleton owning background
    // threads. Avoids static destruction-order hazards and a potential hang on
    // exit should a worker be parked in getnameinfo().
    static Resolver* self = new Resolver();
    return *self;
  }

  void configure(const Options& options)
  {
    std::unique_lock<std::mutex> lock(itsMutex);

    itsEnabled.store(options.enabled, std::memory_order_relaxed);
    itsTimeoutMs.store(options.timeout_ms, std::memory_order_relaxed);
    itsPositiveTtl.store(options.positive_ttl_s, std::memory_order_relaxed);
    itsNegativeTtl.store(options.negative_ttl_s, std::memory_order_relaxed);

    // The cache size is a startup setting: the cache is created once and then
    // read lock-free off the request thread, so it is not resized afterwards.
    itsCacheSize = std::max<std::size_t>(1, options.cache_size);
    ensureCacheLocked();

    startWorkersLocked(std::max(1U, options.threads));
  }

  void shutdown()
  {
    std::vector<std::thread> workers;
    {
      std::unique_lock<std::mutex> lock(itsMutex);
      if (!itsRunning)
        return;
      itsRunning = false;
      itsStop = true;
      workers.swap(itsWorkers);
    }
    itsCv.notify_all();
    for (auto& w : workers)
      if (w.joinable())
        w.join();
    // itsStop stays true until startWorkersLocked() clears it under the lock when
    // the workers are (lazily) restarted, keeping all access to it lock-guarded.
  }

  std::string getHostName(const std::string& theIP)
  {
    if (!itsEnabled.load(std::memory_order_relaxed))
      return {};

    if (theIP.empty())
      return {};

    // 1. Cache first - the common case for repeat / known-bad clients.
    if (auto cached = cacheFind(theIP))
      return *cached;

    // 2. Cache miss: hand the lookup to a worker and (optionally) wait briefly.
    std::shared_future<std::string> future;
    {
      std::unique_lock<std::mutex> lock(itsMutex);
      startWorkersLocked(1);  // lazily (re)start workers if needed

      auto it = itsInFlight.find(theIP);
      if (it != itsInFlight.end())
      {
        future = it->second->future;
      }
      else
      {
        if (itsInFlight.size() >= kMaxInFlight)
          return {};  // overloaded: degrade to IP-only logging

        auto pending = std::make_shared<Pending>();
        itsInFlight.emplace(theIP, pending);
        itsQueue.push_back(theIP);
        future = pending->future;
        itsCv.notify_one();
      }
    }

    // 3. Wait at most timeout_ms for a fresh result; otherwise degrade instantly.
    const unsigned int timeout_ms = itsTimeoutMs.load(std::memory_order_relaxed);
    if (timeout_ms == 0)
      return {};  // fully asynchronous: never wait, the cache fills in the background

    if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::ready)
      return future.get();

    return {};  // timed out; the worker keeps going and caches the result
  }

 private:
  using Cache = Fmi::Cache::Cache<std::string, CachedHost>;

  Resolver() = default;

  // Lock-free on the request thread: Fmi::Cache is internally thread-safe and the
  // cache object is created once and never replaced.
  std::optional<std::string> cacheFind(const std::string& theIP)
  {
    Cache* cache = itsCache.load(std::memory_order_acquire);
    if (cache == nullptr)
      return std::nullopt;
    auto value = cache->find(theIP);
    if (!value)
      return std::nullopt;
    if (SteadyClock::now() >= value->expires)
      return std::nullopt;  // expired; treat as a miss so it is re-resolved
    return value->name;
  }

  void cacheStore(const std::string& theIP, const std::string& name)
  {
    Cache* cache = itsCache.load(std::memory_order_acquire);
    if (cache == nullptr)
      return;
    const unsigned int ttl = name.empty() ? itsNegativeTtl.load(std::memory_order_relaxed)
                                          : itsPositiveTtl.load(std::memory_order_relaxed);
    cache->upsert(theIP, CachedHost{name, SteadyClock::now() + std::chrono::seconds(ttl)});
  }

  // Caller must hold itsMutex.
  void ensureCacheLocked()
  {
    if (!itsCacheOwner)
    {
      itsCacheOwner = std::make_unique<Cache>(itsCacheSize);
      itsCache.store(itsCacheOwner.get(), std::memory_order_release);
    }
  }

  // Caller must hold itsMutex.
  void startWorkersLocked(unsigned int threads)
  {
    ensureCacheLocked();
    if (!itsRunning)
    {
      itsRunning = true;
      itsStop = false;
    }
    while (itsWorkers.size() < threads)
      itsWorkers.emplace_back([this] { workerLoop(); });
  }

  void workerLoop()
  {
    for (;;)
    {
      std::string ip;
      std::shared_ptr<Pending> pending;
      {
        std::unique_lock<std::mutex> lock(itsMutex);
        itsCv.wait(lock, [this] { return itsStop || !itsQueue.empty(); });
        if (itsStop && itsQueue.empty())
          return;
        ip = std::move(itsQueue.front());
        itsQueue.pop_front();
        auto it = itsInFlight.find(ip);
        if (it == itsInFlight.end())
          continue;  // defensive: should always be present
        pending = it->second;
      }

      // Resolve without holding the lock - this is the call that may block.
      std::string name = blockingLookup(ip);

      // Publish into the cache before satisfying waiters, so that a request that
      // wakes up immediately sees a consistent cache entry.
      cacheStore(ip, name);
      pending->promise.set_value(name);

      {
        std::unique_lock<std::mutex> lock(itsMutex);
        itsInFlight.erase(ip);
      }
    }
  }

  std::mutex itsMutex;
  std::condition_variable itsCv;

  std::atomic<bool> itsEnabled{true};
  std::atomic<unsigned int> itsTimeoutMs{200};
  std::atomic<unsigned int> itsPositiveTtl{3600};
  std::atomic<unsigned int> itsNegativeTtl{60};

  std::unique_ptr<Cache> itsCacheOwner;   // owns lifetime; touched only under itsMutex
  std::atomic<Cache*> itsCache{nullptr};  // published once, read lock-free on hot path
  std::size_t itsCacheSize{10000};

  std::deque<std::string> itsQueue;
  std::unordered_map<std::string, std::shared_ptr<Pending>> itsInFlight;

  std::vector<std::thread> itsWorkers;
  bool itsRunning{false};
  bool itsStop{false};
};

}  // namespace

void configure(const Options& options)
{
  Resolver::instance().configure(options);
}

void shutdown()
{
  Resolver::instance().shutdown();
}

std::string getHostName(const std::string& theIP)
{
  return Resolver::instance().getHostName(theIP);
}

}  // namespace HostInfo
}  // namespace Spine
}  // namespace SmartMet
