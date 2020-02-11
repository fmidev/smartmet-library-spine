#include <boost/test/included/unit_test.hpp>
#include "MultiLanguageStringArray.h"
#include <macgyver/TypeName.h>
#include "Exception.h"

using namespace boost::unit_test;
using libconfig::Setting;
using namespace SmartMet::Spine;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "MultiLanguageStringArray tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(test_multi_language_string_array_1)
{
    std::shared_ptr<libconfig::Config> config(new libconfig::Config);
    auto& root = config->getRoot();
    auto& ml_root = root.add("test", Setting::TypeGroup);

    auto& m_eng = ml_root.add("eng", Setting::TypeArray);
    m_eng.add(Setting::TypeString) = "One";
    m_eng.add(Setting::TypeString) = "Two";
    m_eng.add(Setting::TypeString) = "Three";

    auto& m_fin = ml_root.add("fin", Setting::TypeArray);
    m_fin.add(Setting::TypeString) = "Yksi";
    m_fin.add(Setting::TypeString) = "Kaksi";
    m_fin.add(Setting::TypeString) = "Kolme";

    auto& m_lav = ml_root.add("lav", Setting::TypeArray);
    m_lav.add(Setting::TypeString) = "Viens";
    m_lav.add(Setting::TypeString) = "Divi";
    m_lav.add(Setting::TypeString) = "Trīs";

    MultiLanguageStringArray::Ptr ml_array;
    try {
      ml_array = MultiLanguageStringArray::create("eng", ml_root);
    } catch (const std::exception& err) {
        std::cerr << "C++ exception of type '" << Fmi::current_exception_type()
                  << "':" << err.what()
                  << std::endl;
        BOOST_REQUIRE(false);
    }

    std::vector<std::string> v1;

    BOOST_REQUIRE_NO_THROW(v1 = ml_array->get());
    BOOST_REQUIRE_EQUAL(int(v1.size()), 3);
    BOOST_CHECK_EQUAL(std::string("One"), v1.at(0));
    BOOST_CHECK_EQUAL(std::string("Two"), v1.at(1));
    BOOST_CHECK_EQUAL(std::string("Three"), v1.at(2));

    BOOST_REQUIRE_NO_THROW(v1 = ml_array->get("foo"));
    BOOST_REQUIRE_EQUAL(int(v1.size()), 3);
    BOOST_CHECK_EQUAL(std::string("One"), v1.at(0));
    BOOST_CHECK_EQUAL(std::string("Two"), v1.at(1));
    BOOST_CHECK_EQUAL(std::string("Three"), v1.at(2));

    BOOST_REQUIRE_NO_THROW(v1 = ml_array->get("fin"));
    BOOST_REQUIRE_EQUAL(int(v1.size()), 3);
    BOOST_CHECK_EQUAL(std::string("Yksi"), v1.at(0));
    BOOST_CHECK_EQUAL(std::string("Kaksi"), v1.at(1));
    BOOST_CHECK_EQUAL(std::string("Kolme"), v1.at(2));

    BOOST_REQUIRE_NO_THROW(v1 = ml_array->get("LAV"));
    BOOST_REQUIRE_EQUAL(int(v1.size()), 3);
    BOOST_CHECK_EQUAL(std::string("Viens"), v1.at(0));
    BOOST_CHECK_EQUAL(std::string("Divi"), v1.at(1));
    BOOST_CHECK_EQUAL(std::string("Trīs"), v1.at(2));

    m_fin.add(Setting::TypeString) = "Neljä";
    // Not equal size: should throw an exception
    BOOST_CHECK_THROW(MultiLanguageStringArray::create("eng", ml_root), SmartMet::Spine::Exception);
}
