// ======================================================================
/*!
 * \brief Background, cache-only reverse-DNS (PTR) resolution.
 *
 * See HostInfo.h for the rationale. The implementation is a small singleton
 * resolver consisting of:
 *
 *   - an LRU cache (IP -> host name) with separate positive/negative TTLs,
 *   - a bounded, deduplicated work queue,
 *   - one or more background worker threads that perform the actual blocking
 *     getnameinfo() call off the caller thread and populate the cache.
 *
 * The request thread calls prefetch() at request ingress to schedule a lookup;
 * it never waits. getHostName() is a pure cache read used by diagnostics/admin
 * output: it returns the cached name (or "" for an unknown / failed / not-yet-
 * resolved IP) and never resolves anything inline. A slow or missing PTR can
 * therefore never delay a request thread, and repeat / known-bad IPs cost
 * essentially nothing.
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
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
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

// Default upper bound on simultaneously pending (queued + in-flight) lookups.
// Caps memory and prevents unbounded growth when the resolver is stuck during a
// DNS outage: further prefetch()es are simply dropped until the backlog drains.
// Configurable via dns.maxqueuesize (HostInfo::Options::max_queue_size).
constexpr std::size_t kDefaultMaxQueueSize = 1000;

// One cache entry: the resolved name ("" means "no PTR record") plus its expiry.
struct CachedHost
{
  std::string name;
  SteadyClock::time_point expires;
};

// Normalise an IP string into a stable cache key. Trims surrounding whitespace so
// that an X-Forwarded-For element such as " 1.2.3.4" produces the same key whether
// it is fed by prefetch() or looked up by getHostName().
std::string normalize(const std::string& theIP)
{
  const auto first = theIP.find_first_not_of(" \t");
  if (first == std::string::npos)
    return {};
  const auto last = theIP.find_last_not_of(" \t");
  return theIP.substr(first, last - first + 1);
}

// Perform the actual blocking reverse lookup. Returns "" if the address cannot
// be parsed or has no PTR record. Runs only on a worker thread, never on the
// caller thread, so the lack of a per-call timeout here is harmless.
std::string blockingLookup(const std::string& theIP)
{
  char node[NI_MAXHOST];

  // Try IPv4 first, then IPv6. An unparseable address degrades to "no host name".
  struct sockaddr_in sa4;
  memset(&sa4, 0, sizeof(sa4));
  sa4.sin_family = AF_INET;
  if (inet_pton(AF_INET, theIP.c_str(), &sa4.sin_addr) == 1)
  {
    if (getnameinfo(
            (struct sockaddr*)&sa4, sizeof(sa4), node, sizeof(node), nullptr, 0, NI_NAMEREQD) != 0)
      return {};
    return node;
  }

  struct sockaddr_in6 sa6;
  memset(&sa6, 0, sizeof(sa6));
  sa6.sin6_family = AF_INET6;
  if (inet_pton(AF_INET6, theIP.c_str(), &sa6.sin6_addr) == 1)
  {
    if (getnameinfo(
            (struct sockaddr*)&sa6, sizeof(sa6), node, sizeof(node), nullptr, 0, NI_NAMEREQD) != 0)
      return {};
    return node;
  }

  return {};
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
    itsPositiveTtl.store(options.positive_ttl_s, std::memory_order_relaxed);
    itsNegativeTtl.store(options.negative_ttl_s, std::memory_order_relaxed);

    itsMaxQueueSize = std::max<std::size_t>(1, options.max_queue_size);

    // The cache size is a startup setting: the cache is created once and then
    // read off the caller thread, so it is not resized afterwards.
    itsCacheSize = std::max<std::size_t>(1, options.cache_size);
    ensureCacheLocked();

    if (options.enabled)
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

  void prefetch(const std::string& theIP)
  {
    if (!itsEnabled.load(std::memory_order_relaxed))
      return;

    const std::string ip = normalize(theIP);
    if (ip.empty())
      return;

    // Already cached with a still-valid TTL (positive or negative)? Nothing to do.
    if (cacheFresh(ip))
      return;

    scheduleResolve(ip);
  }

  std::string getHostName(const std::string& theIP)
  {
    if (!itsEnabled.load(std::memory_order_relaxed))
      return {};

    const std::string ip = normalize(theIP);
    if (ip.empty())
      return {};

    // Stale-while-revalidate cache read. The TTL never blanks an entry: an IP
    // that was previously resolved keeps returning its last name even after the
    // TTL has expired, and the stale read itself schedules a background
    // re-resolution so the name is refreshed without ever blocking the caller.
    // A genuine miss (never resolved) returns "" without resolving - new IPs
    // enter the cache only via prefetch() at request ingress.
    auto entry = cacheLookup(ip);
    if (!entry)
      return {};
    if (SteadyClock::now() >= entry->expires)
      scheduleResolve(ip);  // stale: refresh in the background, serve the old name now
    return entry->name;
  }

  // Cache statistics for the overall cache-statistics report. Returns zeroed
  // stats when the cache has not been created yet (resolution never enabled).
  Fmi::Cache::CacheStats statistics() const
  {
    Cache* cache = itsCache.load(std::memory_order_acquire);
    if (cache == nullptr)
      return {};
    return cache->statistics();
  }

 private:
  using Cache = Fmi::Cache::Cache<std::string, CachedHost>;

  Resolver() = default;

  // Enqueue a background reverse lookup for theIP unless one is already pending
  // or the resolver is overloaded. Non-blocking. Caller must NOT hold itsMutex.
  void scheduleResolve(const std::string& theIP)
  {
    std::unique_lock<std::mutex> lock(itsMutex);
    startWorkersLocked(1);  // lazily (re)start workers if needed

    if (itsInFlight.count(theIP) != 0)
      return;  // someone is already resolving this IP
    if (itsInFlight.size() >= itsMaxQueueSize)
      return;  // overloaded: drop the request, the caller logs the IP only

    itsInFlight.insert(theIP);
    itsQueue.push_back(theIP);
    itsCv.notify_one();
  }

  // Look up the raw cache entry (name + expiry) regardless of TTL. Fmi::Cache is
  // internally thread-safe; the cache object is published once via an atomic.
  std::optional<CachedHost> cacheLookup(const std::string& theIP)
  {
    Cache* cache = itsCache.load(std::memory_order_acquire);
    if (cache == nullptr)
      return std::nullopt;
    return cache->find(theIP);
  }

  // True if theIP has a still-valid cache entry (positive or negative).
  bool cacheFresh(const std::string& theIP)
  {
    Cache* cache = itsCache.load(std::memory_order_acquire);
    if (cache == nullptr)
      return false;
    auto value = cache->find(theIP);
    if (!value)
      return false;
    return SteadyClock::now() < value->expires;
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
      {
        std::unique_lock<std::mutex> lock(itsMutex);
        itsCv.wait(lock, [this] { return itsStop || !itsQueue.empty(); });
        if (itsStop && itsQueue.empty())
          return;
        ip = std::move(itsQueue.front());
        itsQueue.pop_front();
      }

      // Resolve without holding the lock - this is the call that may block.
      const std::string name = blockingLookup(ip);

      cacheStore(ip, name);

      {
        std::unique_lock<std::mutex> lock(itsMutex);
        itsInFlight.erase(ip);
      }
    }
  }

  std::mutex itsMutex;
  std::condition_variable itsCv;

  std::atomic<bool> itsEnabled{true};
  std::atomic<unsigned int> itsPositiveTtl{3600};
  std::atomic<unsigned int> itsNegativeTtl{60};

  std::unique_ptr<Cache> itsCacheOwner;   // owns lifetime; touched only under itsMutex
  std::atomic<Cache*> itsCache{nullptr};  // published once for safe lock-guarded reads
  std::size_t itsCacheSize{200000};

  std::deque<std::string> itsQueue;
  std::unordered_set<std::string> itsInFlight;
  std::size_t itsMaxQueueSize{kDefaultMaxQueueSize};  // touched only under itsMutex

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

void prefetch(const std::string& theIP)
{
  Resolver::instance().prefetch(theIP);
}

std::string getHostName(const std::string& theIP)
{
  return Resolver::instance().getHostName(theIP);
}

Fmi::Cache::CacheStats getCacheStats()
{
  return Resolver::instance().statistics();
}

}  // namespace HostInfo
}  // namespace Spine
}  // namespace SmartMet
