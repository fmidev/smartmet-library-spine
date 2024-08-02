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
  }
};

std::shared_ptr<libconfig::Config> create_config()
{
  std::shared_ptr<libconfig::Config> config(new TestConfig);
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

  auto raw = create_config();

  ip::IPFilter theFilter(raw);

  BOOST_CHECK_EQUAL(theFilter.match("192.169.1.1"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.169.100.100"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.169.255.0"), true);
}

BOOST_AUTO_TEST_CASE(exact_filter)
{
  BOOST_TEST_MESSAGE("Testing ExactMatch - filter");

  auto raw = create_config();

  ip::IPFilter theFilter(raw);

  BOOST_CHECK_EQUAL(theFilter.match("192.168.1.1"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.168.10.10"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.168.11.11"), false);
}

BOOST_AUTO_TEST_CASE(range_filter)
{
  BOOST_TEST_MESSAGE("Testing RangeMatch - filter");

  auto raw = create_config();

  ip::IPFilter theFilter(raw);

  BOOST_CHECK_EQUAL(theFilter.match("192.170.10.5"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.170.10.9"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.170.10.10"), true);

  BOOST_CHECK_EQUAL(theFilter.match("192.170.10.11"), false);
}

BOOST_AUTO_TEST_SUITE_END()
