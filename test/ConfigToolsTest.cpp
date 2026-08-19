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

BOOST_AUTO_TEST_CASE(parse_size_setting)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing parseSize");

  libconfig::Config cfg;
  auto& root = cfg.getRoot();
  root.add("small", Setting::TypeInt) = 1000;
  root.add("zero", Setting::TypeInt) = 0;
  root.add("big", Setting::TypeInt64) = 34359738368LL;
  root.add("digits", Setting::TypeString) = "34359738368";
  root.add("longdigits", Setting::TypeString) = "34359738368L";
  root.add("gigas", Setting::TypeString) = "32G";
  root.add("gigabytes", Setting::TypeString) = "32GB";
  root.add("gibibytes", Setting::TypeString) = "32 GiB";
  root.add("megas", Setting::TypeString) = "512M";
  root.add("fraction", Setting::TypeString) = "1.5G";
  root.add("negative", Setting::TypeInt) = -1;
  root.add("negativebig", Setting::TypeInt64) = -1LL;
  root.add("garbage", Setting::TypeString) = "32 gigs";
  root.add("boolean", Setting::TypeBoolean) = true;
  root.add("floating", Setting::TypeFloat) = 1.5;

  BOOST_CHECK_EQUAL(1000UL, parseSize(root["small"]));
  BOOST_CHECK_EQUAL(0UL, parseSize(root["zero"]));
  BOOST_CHECK_EQUAL(32UL << 30, parseSize(root["big"]));
  BOOST_CHECK_EQUAL(32UL << 30, parseSize(root["digits"]));
  BOOST_CHECK_EQUAL(32UL << 30, parseSize(root["longdigits"]));
  BOOST_CHECK_EQUAL(32UL << 30, parseSize(root["gigas"]));
  BOOST_CHECK_EQUAL(32UL << 30, parseSize(root["gigabytes"]));
  BOOST_CHECK_EQUAL(32UL << 30, parseSize(root["gibibytes"]));
  BOOST_CHECK_EQUAL(512UL << 20, parseSize(root["megas"]));
  BOOST_CHECK_EQUAL(3UL << 29, parseSize(root["fraction"]));

  BOOST_CHECK_THROW(parseSize(root["negative"]), Fmi::Exception);
  BOOST_CHECK_THROW(parseSize(root["negativebig"]), Fmi::Exception);
  BOOST_CHECK_THROW(parseSize(root["garbage"]), Fmi::Exception);
  BOOST_CHECK_THROW(parseSize(root["boolean"]), Fmi::Exception);
  BOOST_CHECK_THROW(parseSize(root["floating"]), Fmi::Exception);
}

BOOST_AUTO_TEST_CASE(lookup_size_setting)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing lookupSizeSetting");

  libconfig::Config cfg;
  auto& root = cfg.getRoot();
  auto& cache = root.add("cache", Setting::TypeGroup);
  cache.add("memory_bytes", Setting::TypeString) = "32G";
  cache.add("filesystem_bytes", Setting::TypeInt64) = 2147483648LL;

  std::size_t value = 0;

  BOOST_REQUIRE(lookupSizeSetting(cfg, value, "cache.memory_bytes"));
  BOOST_CHECK_EQUAL(32UL << 30, value);

  BOOST_REQUIRE(lookupSizeSetting(cfg, value, "cache.filesystem_bytes"));
  BOOST_CHECK_EQUAL(2UL << 30, value);

  // A missing setting must leave the value alone
  value = 12345;
  BOOST_CHECK(!lookupSizeSetting(cfg, value, "cache.missing_bytes"));
  BOOST_CHECK_EQUAL(12345UL, value);

  // Default value variant
  BOOST_CHECK_EQUAL(32UL << 30, lookupSizeSetting(cfg, "cache.memory_bytes", 100UL));
  BOOST_CHECK_EQUAL(100UL, lookupSizeSetting(cfg, "cache.missing_bytes", 100UL));
}

BOOST_AUTO_TEST_CASE(lookup_size_setting_override)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigTools] Testing lookupSizeSetting host overrides");

  // A host specific override must win over the global default
  const std::string hostname = boost::asio::ip::host_name();

  libconfig::Config cfg;
  auto& root = cfg.getRoot();
  root.add("memory_bytes", Setting::TypeString) = "1G";

  auto& overrides = root.add("overrides", Setting::TypeList);
  auto& group = overrides.add(Setting::TypeGroup);
  auto& names = group.add("name", Setting::TypeArray);
  names.add(Setting::TypeString) = hostname;
  group.add("memory_bytes", Setting::TypeString) = "32G";

  std::size_t value = 0;
  BOOST_REQUIRE(lookupSizeSetting(cfg, value, "memory_bytes"));
  BOOST_CHECK_EQUAL(32UL << 30, value);

  // A setting without an override falls back to the global value
  root.add("filesystem_bytes", Setting::TypeString) = "2G";
  BOOST_REQUIRE(lookupSizeSetting(cfg, value, "filesystem_bytes"));
  BOOST_CHECK_EQUAL(2UL << 30, value);
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
