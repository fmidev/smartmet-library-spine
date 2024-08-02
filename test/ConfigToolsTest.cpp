#include "ConfigTools.h"
#include <boost/test/included/unit_test.hpp>
#include <cstdlib>
// #include <cstring>
#include <macgyver/DebugTools.h>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "ConfigTools tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));

  // Environment variable USER is not defined in Circle-CI environment.
  // Define it here in this case to avoid test failure
  if (!getenv("USER"))
  {
    const char* tmp = "USER=unknown";
    putenv(const_cast<char*>(tmp));
  }

  return NULL;
}

namespace
{
class TestConfig : public libconfig::Config
{
 public:
  TestConfig()
  {
    using libconfig::Setting;
    auto& root = getRoot();
    root.add("ROOT", Setting::TypeString) = "/etc";
    root.add("BASEDIR", Setting::TypeString) = "$(ROOT)/base";
    root.add("USER", Setting::TypeString) = "$(USER)";
  }
};

std::shared_ptr<libconfig::Config> create_config()
{
  std::shared_ptr<libconfig::Config> config(new TestConfig);
  SmartMet::Spine::expandVariables(*config);
  return config;
}
}  // namespace

BOOST_AUTO_TEST_CASE(expansion)
{
  using namespace SmartMet;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing variable expansion");

  auto cfg = SHOW_EXCEPTIONS(create_config());

  std::string value;

  BOOST_REQUIRE_NO_THROW(value = SHOW_EXCEPTIONS(cfg->lookup("ROOT").c_str()));
  BOOST_CHECK_EQUAL("/etc", value);

  BOOST_REQUIRE_NO_THROW(value = SHOW_EXCEPTIONS(cfg->lookup("BASEDIR").c_str()));
  BOOST_CHECK_EQUAL("/etc/base", value);

  BOOST_REQUIRE_NO_THROW(value = SHOW_EXCEPTIONS(cfg->lookup("USER").c_str()));
  BOOST_CHECK_NE("", value);
}
