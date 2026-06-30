// ======================================================================
/*!
 * \brief Reverse-DNS (PTR) resolution of client IP addresses.
 *
 * Resolving a client IP to a host name for diagnostics/admin output used to be a
 * plain, synchronous getnameinfo() call on the request thread. When the resolver
 * was slow or the client IP had no PTR record, that lookup blocked for the full
 * DNS timeout (~5 s) before the caller could continue, so a single slow PTR
 * ruined the latency of the whole request.
 *
 * This interface removes the lookup from the caller's thread entirely:
 *
 *   - resolution runs only on background worker threads, never on the caller's
 *     thread,
 *   - results are cached (LRU) with separate positive/negative TTLs, so repeat
 *     clients and known-bad IPs cost ~0 and failures are not retried,
 *   - the request thread merely prefetch()es the client IP at request ingress
 *     (non-blocking enqueue),
 *   - getHostName() is a pure cache read: it returns the cached name, "" for a
 *     cached failure, or "" if the IP has not been resolved yet. It never
 *     resolves, never enqueues and never blocks,
 *   - resolution can be disabled entirely via configure().
 */
// ======================================================================

#pragma once
#include <macgyver/CacheStats.h>
#include <string>

namespace SmartMet
{
namespace Spine
{
namespace HostInfo
{
// Resolver policy. Configured once at startup (see Reactor); the defaults keep
// the historical behaviour of logging client host names, but without the
// blocking-on-slow-PTR latency footgun.
struct Options
{
  bool enabled = true;                 // false => prefetch() is a no-op, getHostName() returns ""
  unsigned int cache_size = 200000;    // max cached IP -> host name entries (LRU); startup-only
  unsigned int positive_ttl_s = 3600;  // how long a successful lookup stays cached
  unsigned int negative_ttl_s = 60;    // how long a failed lookup stays cached
  unsigned int threads = 1;            // number of background resolver threads
};

// Configure the resolver. Safe to call once at startup before request handling
// begins; may be called again (e.g. from tests) to change the policy. The cache
// size only takes effect on the first call (the cache is not resized afterwards).
void configure(const Options& options);

// Stop the background resolver threads. Idempotent; mainly for clean shutdown
// and tests. prefetch() transparently restarts the workers if called again.
void shutdown();

// Non-blocking ingress feed: schedule a background reverse lookup for theIP if
// it is not already cached (with a still-valid TTL) or in flight. Does nothing
// when resolution is disabled or the resolver is overloaded. Never blocks.
void prefetch(const std::string& theIP);

// Cache-only read: return the cached host name for theIP, "" for a cached
// failure, or "" if theIP has not been resolved yet. Never resolves, never
// enqueues, never blocks. Feed IPs via prefetch() to populate the cache.
std::string getHostName(const std::string& theIP);

// Statistics of the reverse-DNS LRU cache, for inclusion in the server's overall
// cache-statistics report (see Reactor::getCacheStats). Returns zeroed stats if
// resolution is disabled or the cache has not been created yet.
Fmi::Cache::CacheStats getCacheStats();

}  // namespace HostInfo
}  // namespace Spine
}  // namespace SmartMet
