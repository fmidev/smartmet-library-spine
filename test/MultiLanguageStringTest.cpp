#include "Exception.h"
#include "MultiLanguageString.h"
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "MultiLanguageString tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(test_multi_language_string_1)
{
  using libconfig::Setting;
  using namespace SmartMet::Spine;

  BOOST_TEST_MESSAGE("+ [Parse from configuration]");

  boost::shared_ptr<libconfig::Config> config(new libconfig::Config);
  auto& root = config->getRoot();
  auto& ml_root = root.add("test", Setting::TypeGroup);
  ml_root.add("eng", Setting::TypeString) = "An example";
  ml_root.add("fin", Setting::TypeString) = "Esimerkki";
  ml_root.add("LAV", Setting::TypeString) = "Piemērs";

  MultiLanguageStringP ml_string;
  try
  {
    ml_string = MultiLanguageString::create("Eng", ml_root);
  }
  catch (const std::runtime_error& err)
  {
    std::cerr << "C++ exception: " << err.what() << std::endl;
    BOOST_REQUIRE(false);
  }

  BOOST_CHECK_EQUAL(std::string("eng"), ml_string->get_default_language());
  BOOST_CHECK_EQUAL(std::string("An example"), ml_string->get());
  BOOST_CHECK_EQUAL(std::string("An example"), ml_string->get("eng"));
  BOOST_CHECK_EQUAL(std::string("An example"), ml_string->get("eNG"));
  BOOST_CHECK_EQUAL(std::string("Piemērs"), ml_string->get("lav"));
  BOOST_CHECK_EQUAL(std::string("An example"), ml_string->get("foo"));
}

BOOST_AUTO_TEST_CASE(test_multi_language_string_2)
{
  using libconfig::Setting;
  using namespace SmartMet::Spine;

  BOOST_TEST_MESSAGE("+ [Parse from configuration: default language translation missing]");

  boost::shared_ptr<libconfig::Config> config(new libconfig::Config);
  auto& root = config->getRoot();
  auto& ml_root = root.add("test", Setting::TypeGroup);
  ml_root.add("eng", Setting::TypeString) = "An example";
  ml_root.add("fin", Setting::TypeString) = "Esimerkki";
  ml_root.add("LAV", Setting::TypeString) = "Piemērs";

  BOOST_REQUIRE_THROW(MultiLanguageString::create("rus", ml_root), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_multi_language_string_3)
{
  using libconfig::Setting;
  using namespace SmartMet::Spine;

  BOOST_TEST_MESSAGE("+ [Parse from configuration: string provided instead of a group]");

  boost::shared_ptr<libconfig::Config> config(new libconfig::Config);
  auto& root = config->getRoot();
  auto& test = root.add("test", Setting::TypeString);
  test = "foobar";

  MultiLanguageStringP ml_string;
  BOOST_REQUIRE_NO_THROW(ml_string = MultiLanguageString::create("rus", test));
  BOOST_CHECK_EQUAL(std::string("rus"), ml_string->get_default_language());
  BOOST_CHECK_EQUAL(std::string("foobar"), ml_string->get());
  BOOST_CHECK_EQUAL(std::string("foobar"), ml_string->get("eng"));
  BOOST_CHECK_EQUAL(std::string("foobar"), ml_string->get());
}
