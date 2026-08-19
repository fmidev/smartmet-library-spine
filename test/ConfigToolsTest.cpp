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

BOOST_AUTO_TEST_CASE(config_hash_basic)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing config_hash returns non-empty base64 string");

  auto cfg = create_config();
  std::string hash;
  BOOST_REQUIRE_NO_THROW(hash = config_hash(*cfg));
  BOOST_CHECK(!hash.empty());
}

BOOST_AUTO_TEST_CASE(config_hash_deterministic)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing config_hash is deterministic");

  auto cfg1 = create_config();
  auto cfg2 = create_config();
  BOOST_CHECK_EQUAL(config_hash(*cfg1), config_hash(*cfg2));
}

BOOST_AUTO_TEST_CASE(config_hash_differs_on_change)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing config_hash changes when value changes");

  libconfig::Config cfg1;
  cfg1.getRoot().add("x", Setting::TypeInt) = 1;

  libconfig::Config cfg2;
  cfg2.getRoot().add("x", Setting::TypeInt) = 2;

  BOOST_CHECK_NE(config_hash(cfg1), config_hash(cfg2));
}

BOOST_AUTO_TEST_CASE(config_hash_setting)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing config_hash on individual settings");

  libconfig::Config cfg;
  auto& root = cfg.getRoot();
  root.add("a", Setting::TypeInt) = 10;
  root.add("b", Setting::TypeString) = "hello";

  std::string hash_a = config_hash(root["a"]);
  std::string hash_b = config_hash(root["b"]);

  BOOST_CHECK(!hash_a.empty());
  BOOST_CHECK(!hash_b.empty());
  BOOST_CHECK_NE(hash_a, hash_b);
}

BOOST_AUTO_TEST_CASE(config_hash_all_types)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing config_hash with all scalar types");

  libconfig::Config cfg;
  auto& root = cfg.getRoot();
  root.add("i", Setting::TypeInt) = 42;
  root.add("i64", Setting::TypeInt64) = 123456789LL;
  root.add("f", Setting::TypeFloat) = 3.14;
  root.add("s", Setting::TypeString) = "test";
  root.add("b", Setting::TypeBoolean) = true;
  auto& grp = root.add("g", Setting::TypeGroup);
  grp.add("nested", Setting::TypeInt) = 1;
  auto& arr = root.add("a", Setting::TypeArray);
  arr.add(Setting::TypeInt) = 10;
  arr.add(Setting::TypeInt) = 20;
  auto& lst = root.add("l", Setting::TypeList);
  lst.add(Setting::TypeString) = "item";

  std::string hash;
  BOOST_REQUIRE_NO_THROW(hash = config_hash(cfg));
  BOOST_CHECK(!hash.empty());

  // Hash must be stable
  BOOST_CHECK_EQUAL(hash, config_hash(cfg));
}
