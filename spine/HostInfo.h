// ======================================================================
/*!
 * \brief Reverse-DNS (PTR) resolution of client IP addresses.
 *
 * Resolving a client IP to a host name for request/access logging used to be a
 * plain, synchronous getnameinfo() call on the request thread. When the
 * resolver was slow or the client IP had no PTR record, that lookup blocked for
 * the full DNS timeout (~5 s) *before* the request was served, so a single slow
 * PTR ruined the latency of the whole request.
 *
 * This interface keeps the same simple getHostName() contract but guarantees it
 * can never stall request handling:
 *
 *   - results are cached (LRU) with separate positive/negative TTLs, so repeat
 *     clients and known-bad IPs cost ~0,
 *   - lookups run on a background resolver thread, never on the caller's thread,
 *   - the caller waits at most a short, configurable timeout for a fresh result
 *     and otherwise degrades instantly to "" (log the IP, no host name),
 *   - resolution can be disabled entirely via configure().
 */
// ======================================================================

#pragma once
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
  bool enabled = true;              // false => getHostName() returns "" with no lookup at all
  unsigned int timeout_ms = 200;    // max time a caller waits for a fresh lookup; 0 => fully async
  unsigned int cache_size = 10000;  // max cached IP -> host name entries (LRU)
  unsigned int positive_ttl_s = 3600;  // how long a successful lookup stays cached
  unsigned int negative_ttl_s = 60;    // how long a failed/timed-out lookup stays cached
  unsigned int threads = 1;            // number of background resolver threads
};

// Configure the resolver. Safe to call once at startup before request handling
// begins; may be called again (e.g. from tests) to change the policy.
void configure(const Options& options);

// Stop the background resolver threads. Idempotent; mainly for clean shutdown
// and tests. getHostName() transparently restarts the workers if called again.
void shutdown();

// Resolve theIP to a host name. Returns "" when resolution is disabled, the IP
// has no PTR record, or no result is cached within the configured timeout.
// Never blocks the caller longer than Options::timeout_ms.
std::string getHostName(const std::string& theIP);

}  // namespace HostInfo
}  // namespace Spine
}  // namespace SmartMet
