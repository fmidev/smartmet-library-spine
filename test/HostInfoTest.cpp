#include "HostInfo.h"
#include <regression/tframe.h>
#include <chrono>
#include <thread>

//! Protection against conflicts with global functions
namespace HostInfoTest
{
namespace HI = SmartMet::Spine::HostInfo;

// ----------------------------------------------------------------------
/*!
 * \brief When resolution is disabled, getHostName must return "" with no lookup
 */
// ----------------------------------------------------------------------

void disabled()
{
  HI::Options options;
  options.enabled = false;
  HI::configure(options);

  auto ret = HI::getHostName("193.166.221.20");
  if (!ret.empty())
    TEST_FAILED("Disabled resolver should return empty, got '" + ret + "'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief An unparseable / non-IPv4 address resolves to "" (negative result)
 */
// ----------------------------------------------------------------------

void invalid_ip()
{
  HI::Options options;
  options.enabled = true;
  options.timeout_ms = 2000;
  HI::configure(options);

  for (const std::string& ip : {std::string("not-an-ip"), std::string("999.1.2.3")})
  {
    auto ret = HI::getHostName(ip);
    if (!ret.empty())
      TEST_FAILED("Invalid IP '" + ip + "' should return empty, got '" + ret + "'");
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Repeated lookups are served consistently from the cache
 *
 * We do not assert a specific host name (that depends on the resolver), only
 * that a bounded synchronous lookup followed by a second lookup yields the same
 * answer - i.e. the cache returns a stable result.
 */
// ----------------------------------------------------------------------

void resolve_and_cache()
{
  HI::Options options;
  options.enabled = true;
  options.timeout_ms = 5000;  // generous: behave synchronously for the test
  HI::configure(options);

  auto first = HI::getHostName("127.0.0.1");
  auto second = HI::getHostName("127.0.0.1");

  if (first != second)
    TEST_FAILED("Cached lookup should be stable: first='" + first + "' second='" + second + "'");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief With timeout_ms == 0 a cold lookup never blocks: it returns "" at once
 *        and the result is filled into the cache in the background.
 */
// ----------------------------------------------------------------------

void async_never_blocks()
{
  HI::Options options;
  options.enabled = true;
  options.timeout_ms = 0;  // fully asynchronous
  HI::configure(options);

  // First contact with this IP: cache is empty, so the async path returns ""
  // immediately without waiting for the resolver.
  auto immediate = HI::getHostName("127.0.0.1");
  if (!immediate.empty())
    TEST_FAILED("Async cold lookup should return empty immediately, got '" + immediate + "'");

  // Give the background worker a moment; a subsequent lookup may now be cached.
  // We only require that it does not crash or hang - the value itself depends on
  // whether 127.0.0.1 has a PTR record in this environment.
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  (void)HI::getHostName("127.0.0.1");

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
    TEST(invalid_ip);
    TEST(resolve_and_cache);
    TEST(async_never_blocks);
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
