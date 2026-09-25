#include "IPFilter.h"
#include <boost/test/included/unit_test.hpp>
#include <iostream>

using namespace boost::unit_test;

namespace ip = SmartMet::Spine::IPFilter;

namespace
{
class TestConfig : public libconfig::Config
{
 public:
  TestConfig()
  {
    using libconfig::Setting;
    auto& root = getRoot();
    auto& ep1v = root.add("ip_filters", Setting::TypeArray);
    ep1v.add(Setting::TypeString) = "192.168.1.1";
    ep1v.add(Setting::TypeString) = "192.168.10.10";
    ep1v.add(Setting::TypeString) = "192.169.*.*";
    ep1v.add(Setting::TypeString) = "192.170.10.5-10";
    ep1v.add(Setting::TypeString) = "192.171.8-15.*";
  }
};

const TestConfig& create_config()
{
  static const TestConfig config;
  return config;
}
}  // namespace

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "IP Filter tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_SUITE(brainstorm_ipfilter_tests)

BOOST_AUTO_TEST_CASE(any_filter)
{
  BOOST_TEST_MESSAGE("Testing AnyMatch - filter");

  const auto& raw = create_config();

  const ip::IPFilter theFilter = *ip::IPFilter::fromConfig(raw, "ip_filters");

  BOOST_CHECK_EQUAL(theFilter.match("192.169.1.1"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.169.100.100"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.169.255.0"), true);
}

BOOST_AUTO_TEST_CASE(exact_filter)
{
  BOOST_TEST_MESSAGE("Testing ExactMatch - filter");

  const auto& raw = create_config();

  const ip::IPFilter theFilter = *ip::IPFilter::fromConfig(raw, "ip_filters");

  BOOST_CHECK_EQUAL(theFilter.match("192.168.1.1"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.168.10.10"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.168.11.11"), false);
}

BOOST_AUTO_TEST_CASE(range_filter)
{
  BOOST_TEST_MESSAGE("Testing RangeMatch - filter");

  const auto& raw = create_config();

  const ip::IPFilter theFilter = *ip::IPFilter::fromConfig(raw, "ip_filters");

  BOOST_CHECK_EQUAL(theFilter.match("192.170.10.5"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.170.10.9"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.170.10.10"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.170.10.11"), false);

  BOOST_CHECK_EQUAL(theFilter.match("192.171.8.1"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.171.15.45"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.171.16.1"), false);
}

BOOST_AUTO_TEST_CASE(malformed_address)
{
  BOOST_TEST_MESSAGE("Testing malformed addresses - filter");

  const auto& raw = create_config();

  const ip::IPFilter theFilter = *ip::IPFilter::fromConfig(raw, "ip_filters");

  // An address with more than four dotted fields must not match (and must not read the
  // internal 4-element filter array out of bounds).
  BOOST_CHECK_EQUAL(theFilter.match("192.168.1.1.5.6"), false);
  BOOST_CHECK_EQUAL(theFilter.match("192.168.10.10.0"), false);

  // A truncated address must not match a prefix of a filter rule.
  BOOST_CHECK_EQUAL(theFilter.match("192"), false);
  BOOST_CHECK_EQUAL(theFilter.match("192.168"), false);
  BOOST_CHECK_EQUAL(theFilter.match("192.168.1"), false);

  // An IPv6 address tokenises to a single field and must fail closed against IPv4 rules.
  BOOST_CHECK_EQUAL(theFilter.match("::1"), false);
  BOOST_CHECK_EQUAL(theFilter.match(""), false);
}

BOOST_AUTO_TEST_CASE(missing_setting)
{
  BOOST_TEST_MESSAGE("Testing missing setting");
  BOOST_CHECK(ip::IPFilter::fromConfig(create_config(), "no_such_filters") == nullptr);
}

BOOST_AUTO_TEST_CASE(cidr_and_ipv6)
{
  BOOST_TEST_MESSAGE("Testing CIDR and IPv6 rules");

  ip::IPFilter filter({"10.0.0.0/8", "172.16.0.0/12", "::1", "fd00::/8", " 192.168.1.1 "});

  BOOST_CHECK(filter.match("10.255.3.4"));
  BOOST_CHECK(!filter.match("11.0.0.1"));
  BOOST_CHECK(filter.match("172.31.255.255"));
  BOOST_CHECK(!filter.match("172.32.0.0"));
  BOOST_CHECK(filter.match("::1"));
  BOOST_CHECK(!filter.match("::2"));
  BOOST_CHECK(filter.match("fd12:3456::1"));
  BOOST_CHECK(!filter.match("fe80::1"));
  BOOST_CHECK(filter.match("192.168.1.1"));
  // IPv4-mapped IPv6 is matched as IPv4
  BOOST_CHECK(filter.match("::ffff:10.1.2.3"));
  BOOST_CHECK(!filter.match("::ffff:11.1.2.3"));

  ip::IPFilter any({"*.*.*.*"});
  BOOST_CHECK(any.match("1.2.3.4"));
  BOOST_CHECK(!any.match("::1"));
  BOOST_CHECK(!any.match("unknown"));

  ip::IPFilter all({"0.0.0.0/0"});
  BOOST_CHECK(all.match("255.255.255.255"));
  BOOST_CHECK(!all.match("::2"));

  ip::IPFilter none(std::vector<std::string>{});
  BOOST_CHECK(none.empty());
  BOOST_CHECK(!none.match("127.0.0.1"));
}

BOOST_AUTO_TEST_CASE(malformed_rules)
{
  BOOST_TEST_MESSAGE("Testing malformed rules");

  for (const auto* rule : {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "1.2.3.a", "1.2.3.4-", "a-b.1.1.1",
                           "1.2.3.4/33", "::/129", "1.2.3.4/", "1.2.3.4/x", "*", "localhost",
                           "1.2.3.1-2-3"})
  {
    BOOST_CHECK_THROW(ip::IPFilter({rule}), std::exception);
  }
}

BOOST_AUTO_TEST_CASE(parse_address)
{
  BOOST_TEST_MESSAGE("Testing X-Forwarded-For address parsing");

  BOOST_CHECK_EQUAL(ip::parseAddress(" 1.2.3.4 ")->to_string(), "1.2.3.4");
  BOOST_CHECK_EQUAL(ip::parseAddress("1.2.3.4:8080")->to_string(), "1.2.3.4");
  BOOST_CHECK_EQUAL(ip::parseAddress("[::1]:80")->to_string(), "::1");
  BOOST_CHECK_EQUAL(ip::parseAddress("[fd00::1]")->to_string(), "fd00::1");
  BOOST_CHECK_EQUAL(ip::parseAddress("::ffff:1.2.3.4")->to_string(), "1.2.3.4");
  BOOST_CHECK(!ip::parseAddress("unknown"));
  BOOST_CHECK(!ip::parseAddress(""));
  BOOST_CHECK(!ip::parseAddress("1.2.3.4:"));
  BOOST_CHECK(!ip::parseAddress("1.2.3.4:x"));
  BOOST_CHECK(!ip::parseAddress("[1.2.3.4]"));
  BOOST_CHECK(!ip::parseAddress("[::1"));
  BOOST_CHECK(!ip::parseAddress("1.2.3.4.5"));
}

BOOST_AUTO_TEST_CASE(resolve_client_ip)
{
  BOOST_TEST_MESSAGE("Testing client IP resolution");

  const ip::IPFilter proxies({"10.0.0.0/8"});
  auto resolve = [&](const std::string& peer, const std::optional<std::string>& xff)
  { return ip::resolveClientIP(peer, xff, proxies); };

  // No header or untrusted peer: the header is ignored
  BOOST_CHECK_EQUAL(resolve("1.2.3.4", std::nullopt), "1.2.3.4");
  BOOST_CHECK_EQUAL(resolve("1.2.3.4", std::string("127.0.0.1")), "1.2.3.4");

  // Trusted peer: right-most untrusted address
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("5.6.7.8")), "5.6.7.8");
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("5.6.7.8, 10.1.1.1")), "5.6.7.8");

  // A client-chosen prefix of the chain is not believed
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("127.0.0.1, 5.6.7.8")), "5.6.7.8");
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("garbage, 5.6.7.8")), "5.6.7.8");

  // All trusted: left-most
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("10.2.2.2,10.1.1.1")), "10.2.2.2");

  // Malformed entries never resolve to the proxy itself
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("")), "unknown");
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("5.6.7.8, garbage")), "unknown");
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("5.6.7.8,,10.1.1.1")), "unknown");

  // Ports and brackets are removed
  BOOST_CHECK_EQUAL(resolve("10.0.0.1", std::string("[2001:db8::1]:443")), "2001:db8::1");

  // Backward compatible default: everything IPv4 trusted means the left-most entry
  const ip::IPFilter any({"*.*.*.*"});
  BOOST_CHECK_EQUAL(ip::resolveClientIP("1.2.3.4", std::string("127.0.0.1, 5.6.7.8"), any),
                    "127.0.0.1");
}

BOOST_AUTO_TEST_CASE(frontend_backend_chain)
{
  BOOST_TEST_MESSAGE("Testing spoofing through a trusted frontend");

  // The attacker (1.2.3.4) sends X-Forwarded-For: 127.0.0.1 to a frontend which
  // does not trust it. The frontend forwards the IP it resolved to a backend which
  // trusts the frontend (10.0.0.1). The backend must see the attacker's address.
  const ip::IPFilter frontendTrust({"192.168.0.0/16"});
  const ip::IPFilter backendTrust({"10.0.0.0/8"});
  const ip::IPFilter admin({"127.0.0.1"});

  const auto atFrontend = ip::resolveClientIP("1.2.3.4", std::string("127.0.0.1"), frontendTrust);
  BOOST_CHECK_EQUAL(atFrontend, "1.2.3.4");

  const auto atBackend = ip::resolveClientIP("10.0.0.1", atFrontend, backendTrust);
  BOOST_CHECK_EQUAL(atBackend, "1.2.3.4");
  BOOST_CHECK(!admin.match(atBackend));
}

BOOST_AUTO_TEST_SUITE_END()
