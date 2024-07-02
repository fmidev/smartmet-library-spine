#include "ConfigBase.h"
#include <boost/test/included/unit_test.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "ConfigBase tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
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
    root.add("disabled", Setting::TypeBoolean) = false;
    root.add("id", Setting::TypeString) = "foo";
    root.add("constructor_name", Setting::TypeString) = "create_foo";
    root.add("title", Setting::TypeGroup).add("eng", Setting::TypeString) = "Title";
    root.add("abstract", Setting::TypeGroup).add("eng", Setting::TypeString) = "Abstract";
    root.add("template", Setting::TypeString) = "foo.c2t";
    root.add("defaultLanguage", Setting::TypeString) = "eng";
    root.add("returnTypeNames", Setting::TypeArray).add(Setting::TypeString) = "fooRetVal";
    auto& p1 = root.add("parameters", Setting::TypeList);
    auto& p11 = p1.add(Setting::TypeGroup);
    p11.add("name", Setting::TypeString) = "bbox";
    p11.add("title", Setting::TypeGroup).add("eng", Setting::TypeString) = "title";
    p11.add("abstract", Setting::TypeGroup).add("eng", Setting::TypeString) = "abstract";
    p11.add("xmlType", Setting::TypeString) = "testType";
    p11.add("type", Setting::TypeString) = "double[4]";

    root.add("boundingBox", Setting::TypeArray).add(Setting::TypeString) = "${bbox>defaultBbox}";

    root.add("foo", Setting::TypeString) = "%[parameters.[0].name]";

    auto& ep1 = root.add("handler_params", Setting::TypeList).add(Setting::TypeGroup);
    ep1.add("name", Setting::TypeString) = "defaultBbox";
    auto& ep1v = ep1.add("def", Setting::TypeArray);
    ep1v.add(Setting::TypeFloat) = 20.0;
    ep1v.add(Setting::TypeFloat) = 60.0;
    ep1v.add(Setting::TypeFloat) = 25.0;
    ep1v.add(Setting::TypeFloat) = 65.0;

    root.add("intValue", Setting::TypeInt) = 123456;
    root.add("doubleValue", Setting::TypeFloat) = 3.14159;
    root.add("stringValue", Setting::TypeString) = "foobar";

    auto& ea1 = root.add("intArray", Setting::TypeArray);
    ea1.add(Setting::TypeInt) = 123;
    ea1.add(Setting::TypeInt) = 234;
    ea1.add(Setting::TypeInt) = 345;
    ea1.add(Setting::TypeInt) = 456;
  }
};

std::shared_ptr<libconfig::Config> create_config()
{
  std::shared_ptr<libconfig::Config> config(new TestConfig);
  return config;
}
}  // namespace

BOOST_AUTO_TEST_CASE(get_simple_config_entries)
{
  using namespace SmartMet;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigBase] Testing getting mandatory and optional configuration entries");

  auto raw_config = create_config();
  std::shared_ptr<SmartMet::Spine::ConfigBase> config;

  BOOST_REQUIRE_NO_THROW(
      config.reset(new SmartMet::Spine::ConfigBase(raw_config, "Test configuration")));

  int i1;
  double d1;
  std::string s1;
  libconfig::Setting* w1;

  BOOST_REQUIRE_NO_THROW(i1 = config->get_mandatory_config_param<int>("intValue"));
  BOOST_CHECK_EQUAL(123456, i1);

  BOOST_REQUIRE_NO_THROW(i1 = config->get_optional_config_param<int>("intValue", 654321));
  BOOST_CHECK_EQUAL(123456, i1);

  BOOST_REQUIRE_NO_THROW(i1 = config->get_optional_config_param<int>("intValue2", 654321));
  BOOST_CHECK_EQUAL(654321, i1);

  BOOST_REQUIRE_NO_THROW(d1 = config->get_mandatory_config_param<double>("doubleValue"));
  BOOST_CHECK_CLOSE(3.14159, d1, 1e-8);

  BOOST_REQUIRE_NO_THROW(s1 = config->get_mandatory_config_param<std::string>("stringValue"));
  BOOST_CHECK_EQUAL(std::string("foobar"), s1);

  BOOST_REQUIRE_NO_THROW(
      w1 = &config->get_mandatory_config_param<libconfig::Setting&>("doubleValue"));
  BOOST_CHECK_EQUAL(Setting::TypeFloat, w1->getType());
}

BOOST_AUTO_TEST_CASE(get_config_array_no_nesting)
{
  using namespace SmartMet;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE(
      "+ [ConfigBase] Testing getting configuration array (array directly under root)");

  auto raw_config = create_config();
  std::shared_ptr<SmartMet::Spine::ConfigBase> config;

  BOOST_REQUIRE_NO_THROW(
      config.reset(new SmartMet::Spine::ConfigBase(raw_config, "Test configuration")));

  std::vector<int> a1;
  BOOST_CHECK_NO_THROW(a1 = config->get_mandatory_config_array<int>("intArray"));
  BOOST_CHECK_EQUAL(4, (int)a1.size());
  BOOST_CHECK_EQUAL(123, a1.at(0));
  BOOST_CHECK_EQUAL(234, a1.at(1));
  BOOST_CHECK_EQUAL(345, a1.at(2));
  BOOST_CHECK_EQUAL(456, a1.at(3));
}

BOOST_AUTO_TEST_CASE(get_nested_config_entries)
{
  using namespace SmartMet;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigBase] Testing getting nested cong entries");

  auto raw_config = create_config();
  std::shared_ptr<SmartMet::Spine::ConfigBase> config;

  BOOST_REQUIRE_NO_THROW(
      config.reset(new SmartMet::Spine::ConfigBase(raw_config, "Test configuration")));

  std::string s1;

  BOOST_REQUIRE_NO_THROW(s1 = config->get_mandatory_config_param<std::string>("title.eng"));
  BOOST_CHECK_EQUAL(std::string("Title"), s1);

  BOOST_REQUIRE_NO_THROW(
      s1 = config->get_mandatory_config_param<std::string>("parameters.[0].abstract.eng"));
  BOOST_CHECK_EQUAL(std::string("abstract"), s1);

  libconfig::Setting* w1;
  BOOST_REQUIRE_NO_THROW(
      w1 = &config->get_mandatory_config_param<libconfig::Setting&>("parameters.[0]"));
  BOOST_REQUIRE_NO_THROW(s1 = config->get_mandatory_config_param<std::string>(*w1, "title.eng"));
  BOOST_CHECK_EQUAL(std::string("title"), s1);
}

BOOST_AUTO_TEST_CASE(get_array_deep)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigBase] Testing getting array from internal configuration entries");

  auto raw_config = create_config();
  std::shared_ptr<ConfigBase> config;

  BOOST_REQUIRE_NO_THROW(config.reset(new ConfigBase(raw_config, "Test configuration")));

  std::vector<double> data;
  BOOST_REQUIRE_NO_THROW(data =
                             config->get_mandatory_config_array<double>("handler_params.[0].def"));
  BOOST_CHECK_EQUAL(4, (int)data.size());
  BOOST_CHECK_CLOSE(20.0, data.at(0), 1e-10);
  BOOST_CHECK_CLOSE(60.0, data.at(1), 1e-10);
  BOOST_CHECK_CLOSE(25.0, data.at(2), 1e-10);
  BOOST_CHECK_CLOSE(65.0, data.at(3), 1e-10);
}

BOOST_AUTO_TEST_CASE(test_parameter_redirection_1)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigBase] Testing configuration entry redirect");

  auto raw_config = create_config();
  std::shared_ptr<ConfigBase> config;

  BOOST_REQUIRE_NO_THROW(config.reset(new ConfigBase(raw_config, "Test configuration")));

  std::string value;
  BOOST_REQUIRE_NO_THROW(value = config->get_mandatory_config_param<std::string>("foo"));
  BOOST_REQUIRE_EQUAL(value, "bbox");
}

BOOST_AUTO_TEST_CASE(dump_and_reread_config)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigBase] Dump config to std::ostream and reread it");

  auto raw_config = create_config();
  std::ostringstream stream;
  SmartMet::Spine::ConfigBase::dump_config(stream, *raw_config);

  char fn[80];
  FILE* file;
  strncpy(fn, "/tmp/ConfigBaseTest.XXXXXX", sizeof(fn));
  int fd = mkstemp(fn);
  BOOST_REQUIRE(fd >= 0);

  try
  {
    BOOST_REQUIRE((file = fdopen(fd, "w")) != NULL);
    if (fputs(stream.str().c_str(), file) < 0)
      perror("fputs");
    fclose(file);

    try
    {
      ConfigBase test_config(fn);
      (void)test_config;
      unlink(fn);
    }
    catch (const std::exception& err)
    {
      std::cerr << err.what() << std::endl;
      *fn = 0;
      BOOST_REQUIRE(false);
    }
  }
  catch (...)
  {
    if (*fn)
      unlink(fn);
    throw;
  }
}

namespace
{
void mkfile(const std::string& fn, const std::string& data)
{
  std::ofstream out;
  // out.exceptions(std::ios::failbit | std::ios::badbit);
  out.open(fn.c_str());
  out << data;
}
}  // namespace

BOOST_AUTO_TEST_CASE(test_include_1)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigBase] Include by relative and absolute path");

  char* dn = nullptr;
  char c_dn[256];
  std::string fn1, fn2;

  const auto cleanup = [&]()
  {
    if (fn1 != "")
    {
      unlink(fn1.c_str());
    }
    if (fn2 != "")
    {
      unlink(fn2.c_str());
    }
    if (dn)
    {
      rmdir(dn);
    }
  };

  strncpy(c_dn, "/tmp/smartmet-library-spine-XXXXXX", sizeof(c_dn));
  dn = mkdtemp(c_dn);
  if (!dn)
  {
    perror("Failed to create temporary directory");
    BOOST_REQUIRE(dn);
  }

  try
  {
    fn1 = dn + std::string("/test.conf");
    fn2 = dn + std::string("/test.inc");
    mkfile(fn1, "@include \"test.inc\"\n");
    mkfile(fn2, "foo = \"bar\";\n");

    // Try include by relative path
    std::unique_ptr<ConfigBase> cfg;

    try
    {
      cfg.reset(new ConfigBase(fn1, "Test"));
    }
    catch (const Fmi::Exception& e)
    {
      e.printError();
      BOOST_REQUIRE_NO_THROW(throw);
    }
    catch (...)
    {
      BOOST_REQUIRE_NO_THROW(throw);
    }

    std::string x;
    BOOST_REQUIRE_NO_THROW(x = cfg->get_mandatory_config_param<std::string>("foo"));
    BOOST_CHECK_EQUAL(x, std::string("bar"));

    // Try include by absolute path
    cfg.reset();
    mkfile(fn1, "@include \"" + fn2 + "\"\n");

    try
    {
      cfg.reset(new ConfigBase(fn1, "Test"));
    }
    catch (const Fmi::Exception& e)
    {
      e.printError();
      BOOST_REQUIRE_NO_THROW(throw);
    }
    catch (...)
    {
      BOOST_REQUIRE_NO_THROW(throw);
    }

    BOOST_REQUIRE_NO_THROW(x = cfg->get_mandatory_config_param<std::string>("foo"));
    BOOST_CHECK_EQUAL(x, std::string("bar"));

    cleanup();
  }
  catch (...)
  {
    cleanup();
    throw;
  }
}

BOOST_AUTO_TEST_CASE(test_recursive_include)
{
  using namespace SmartMet::Spine;
  using libconfig::Setting;

  BOOST_TEST_MESSAGE("+ [ConfigBase] Recursive include by relative and absolute path");

  char* dn = nullptr;
  char c_dn[256];
  std::string fn0, fn1, fn2, fn3;

  const auto cleanup = [&]()
  {
    if (fn1 != "")
    {
      unlink(fn1.c_str());
    }
    if (fn2 != "")
    {
      unlink(fn2.c_str());
    }
    if (fn3 != "")
    {
      unlink(fn3.c_str());
    }
    if (fn0 != "")
    {
      rmdir(fn0.c_str());
    }
    if (dn)
    {
      rmdir(dn);
    }
  };

  strncpy(c_dn, "/tmp/smartmet-library-spine-XXXXXX", sizeof(c_dn));
  dn = mkdtemp(c_dn);
  if (!dn)
  {
    perror("Failed to create temporary directory");
    BOOST_REQUIRE(dn);
  }

  try
  {
    fn0 = dn + std::string("/cnf");
    fn1 = dn + std::string("/test.conf");
    fn2 = fn0 + std::string("/test.inc");
    fn3 = fn0 + std::string("/test2.inc");

    mkdir(fn0.c_str(), 0700);
    mkfile(fn1, "@include \"cnf/test.inc\"\n");
    mkfile(fn2, "@include \"cnf/test2.inc\"\n");
    mkfile(fn3, "foo = \"bar\";\n");

    std::unique_ptr<ConfigBase> cfg;

    try
    {
      cfg.reset(new ConfigBase(fn1, "Test"));
    }
    catch (const Fmi::Exception& e)
    {
      e.printError();
      BOOST_REQUIRE_NO_THROW(throw);
    }
    catch (...)
    {
      BOOST_REQUIRE_NO_THROW(throw);
    }

    std::string x;
    BOOST_REQUIRE_NO_THROW(x = cfg->get_mandatory_config_param<std::string>("foo"));
    BOOST_CHECK_EQUAL(x, std::string("bar"));

    // Try second include by absolute path
    cfg.reset();
    mkfile(fn2, "@include \"" + fn3 + "\"\n");

    try
    {
      cfg.reset(new ConfigBase(fn1, "Test"));
    }
    catch (const Fmi::Exception& e)
    {
      e.printError();
      BOOST_REQUIRE_NO_THROW(throw);
    }
    catch (...)
    {
      BOOST_REQUIRE_NO_THROW(throw);
    }

    BOOST_REQUIRE_NO_THROW(x = cfg->get_mandatory_config_param<std::string>("foo"));
    BOOST_CHECK_EQUAL(x, std::string("bar"));

    cleanup();
  }
  catch (...)
  {
    cleanup();
    throw;
  }
}
