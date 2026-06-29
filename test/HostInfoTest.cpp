#include "HostInfo.h"
#include <regression/tframe.h>
#include <chrono>
#include <string>
#include <thread>

//! Protection against conflicts with global functions
namespace HostInfoTest
{
namespace HI = SmartMet::Spine::HostInfo;

// Poll the cache for up to ~2 s waiting for theIP to become resolved (non-empty).
// Resolution is asynchronous, so a freshly prefetched IP is not available at once.
// Returns the cached name (possibly "" if it resolved to a negative result or did
// not finish in time).
std::string waitForResolved(const std::string& theIP)
{
  for (int i = 0; i < 100; ++i)
  {
    auto name = HI::getHostName(theIP);
    if (!name.empty())
      return name;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return HI::getHostName(theIP);
}

// Give the single background worker time to drain a known-negative lookup.
void drain()
{
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

// ----------------------------------------------------------------------
/*!
 * \brief When resolution is disabled, prefetch is a no-op and getHostName is ""
 */
// ----------------------------------------------------------------------

void disabled()
{
  HI::Options options;
  options.enabled = false;
  HI::configure(options);

  HI::prefetch("127.0.0.1");
  auto ret = HI::getHostName("127.0.0.1");
  if (!ret.empty())
    TEST_FAILED("Disabled resolver should return empty, got '" + ret + "'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief getHostName never resolves inline: an IP that was never prefetched
 *        returns "" immediately (cache-only contract).
 */
// ----------------------------------------------------------------------

void cache_only_before_prefetch()
{
  HI::Options options;
  options.enabled = true;
  HI::configure(options);

  // 203.0.113.7 is TEST-NET-3 and has never been prefetched in this run.
  auto ret = HI::getHostName("203.0.113.7");
  if (!ret.empty())
    TEST_FAILED("getHostName must not resolve unknown IPs inline, got '" + ret + "'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief A prefetched IP is resolved in the background and then served from the
 *        cache as a stable value. We do not assert a specific host name (it
 *        depends on the environment's PTR records), only stability.
 */
// ----------------------------------------------------------------------

void prefetch_then_cached()
{
  HI::Options options;
  options.enabled = true;
  HI::configure(options);

  HI::prefetch("127.0.0.1");
  auto first = waitForResolved("127.0.0.1");
  auto second = HI::getHostName("127.0.0.1");

  if (first != second)
    TEST_FAILED("Cached lookup should be stable: first='" + first + "' second='" + second + "'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief An expired entry is not dropped: getHostName keeps returning the
 *        previously resolved name (stale-while-revalidate) and the stale read
 *        itself triggers a background re-resolution.
 *
 * 127.0.0.1 reverse-resolves to "localhost" via /etc/hosts on essentially all
 * Linux hosts, and is uncached until this point (the disabled() test's prefetch
 * was a no-op). We configure a 1 s positive TTL so the entry expires quickly.
 */
// ----------------------------------------------------------------------

void stale_served_and_refreshed()
{
  HI::Options options;
  options.enabled = true;
  options.positive_ttl_s = 1;  // expire quickly
  HI::configure(options);

  HI::prefetch("127.0.0.1");
  auto resolved = waitForResolved("127.0.0.1");

  // Let the 1 s TTL lapse so the entry is now stale.
  std::this_thread::sleep_for(std::chrono::milliseconds(1300));

  // The expired entry must still be served (not blanked out).
  auto stale = HI::getHostName("127.0.0.1");
  if (stale != resolved)
    TEST_FAILED("Expired entry must still serve the previously resolved name: got '" + stale +
                "', expected '" + resolved + "'");

  // The stale read above also scheduled a background re-resolution; after it
  // completes the entry is fresh again and still serves the same name.
  auto refreshed = waitForResolved("127.0.0.1");
  if (refreshed != resolved)
    TEST_FAILED("Re-resolved entry should match: got '" + refreshed + "', expected '" + resolved +
                "'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Unparseable / invalid addresses resolve to a cached negative ("").
 */
// ----------------------------------------------------------------------

void negative_cached()
{
  HI::Options options;
  options.enabled = true;
  HI::configure(options);

  for (const std::string& ip : {std::string("not-an-ip"), std::string("999.1.2.3")})
  {
    HI::prefetch(ip);
    drain();
    auto ret = HI::getHostName(ip);
    if (!ret.empty())
      TEST_FAILED("Invalid IP '" + ip + "' should resolve to empty, got '" + ret + "'");
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief IPv6 literals are accepted (parsed, not rejected) and produce a stable
 *        cached result. The loopback ::1 may or may not have a PTR record, so we
 *        only require that it does not crash or hang and is stable.
 */
// ----------------------------------------------------------------------

void ipv6()
{
  HI::Options options;
  options.enabled = true;
  HI::configure(options);

  HI::prefetch("::1");
  auto first = waitForResolved("::1");
  auto second = HI::getHostName("::1");

  if (first != second)
    TEST_FAILED("IPv6 cached lookup should be stable: first='" + first + "' second='" + second +
                "'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * The actual test suite
 */
// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(disabled);
    TEST(cache_only_before_prefetch);
    TEST(stale_served_and_refreshed);
    TEST(prefetch_then_cached);
    TEST(negative_cached);
    TEST(ipv6);
  }
};

}  // namespace HostInfoTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "HostInfo tester" << endl << "===============" << endl;
  HostInfoTest::tests t;
  int result = t.run();
  SmartMet::Spine::HostInfo::shutdown();  // join background resolver threads cleanly
  return result;
}

// ======================================================================
