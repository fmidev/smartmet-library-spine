// ======================================================================
/*!
 * \file
 * \brief Regression tests for class Convenience
 */
// ======================================================================

#include "Convenience.h"
#include "Exception.h"
#include <boost/test/included/unit_test.hpp>
#include <sstream>

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Convenience tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

template <typename T>
std::string tostr(const T& theValue)
{
  std::ostringstream out;
  out << theValue;
  return out.str();
}

BOOST_AUTO_TEST_CASE(test_optional)
{
  using namespace SmartMet::Spine;

  BOOST_TEST_MESSAGE("+ [Convenience] Testing optional_*");

  const char* null = NULL;

  BOOST_CHECK_EQUAL(100, optional_int(null, 100));
  BOOST_CHECK_EQUAL(10, optional_int("10", 100));
  BOOST_CHECK_EQUAL(10.0, optional_double("10", 100));
  BOOST_CHECK_EQUAL(100.0, optional_double(null, 100));
  BOOST_CHECK_THROW(optional_double("100a", 100), SmartMet::Spine::Exception);

  BOOST_CHECK_EQUAL(100ul, optional_unsigned_long(null, 100ul));
  BOOST_CHECK_EQUAL(10ul, optional_unsigned_long("10", 100ul));

  BOOST_CHECK_EQUAL(100ul, optional_size(null, 100ul));
  BOOST_CHECK_EQUAL(10ul, optional_size("10", 100ul));
}

// ----------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(test_required)
{
  using namespace SmartMet::Spine;

  BOOST_TEST_MESSAGE("+ [Convenience] Testing required_*");

  const char* null = NULL;

  BOOST_CHECK_EQUAL(10, required_int("10", "error"));
  BOOST_CHECK_EQUAL(10ul, required_unsigned_long("10", "error"));
  BOOST_CHECK_EQUAL(10ul, required_size("10", "error"));
  BOOST_CHECK_EQUAL(10.0, required_double("10", "error"));
  BOOST_CHECK_THROW(required_int(null, "error"), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(required_double("100a", "error"), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_str_iless)
{
  BOOST_TEST_MESSAGE("+ [Convenience] Testing str_iless()");

  using SmartMet::Spine::str_iless;

  std::locale locale("fi_FI.utf8");
  BOOST_CHECK(str_iless("foo", "foobar", locale));
  BOOST_CHECK(str_iless("foo", "fOObar", locale));
  BOOST_CHECK(not str_iless("äÖ", "äÄ", locale));
  BOOST_CHECK(not str_iless("aB", "AB", locale));
  BOOST_CHECK(not str_iless("AB", "aB", locale));
  BOOST_CHECK(not str_iless("äö", "Äö", locale));
  // FIXME: jostain syistä seuraava testi ei toimi oikein
  // BOOST_CHECK(not str_iless("Äö", "äö", locale));
}

#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>

BOOST_AUTO_TEST_CASE(test_str_iequal)
{
  BOOST_TEST_MESSAGE("+ [Convenience] Testing str_equal()");

  using SmartMet::Spine::str_iequal;

  std::locale locale("fi_FI.utf8");
  BOOST_CHECK(not str_iequal("foo", "bar", locale));
  BOOST_CHECK(not str_iequal("foo", "foobar", locale));
  BOOST_CHECK(str_iequal("foo", "fOO", locale));
  BOOST_CHECK(str_iequal("äÖ", "äÖ", locale));
  // FIXME: seuraava testi ei toimi oikein
  // BOOST_CHECK(str_iequal("äÖ", "ÄÖ", locale));
  // BOOST_CHECK(str_iequal("ÄÖ", "äÖ", locale));
}

BOOST_AUTO_TEST_CASE(test_CaseInsensitiveLess)
{
  BOOST_TEST_MESSAGE("+ [Convenience] Testing CaseInsensitiveLess");

  using SmartMet::Spine::CaseInsensitiveLess;

  std::locale locale("fi_FI.utf8");
  CaseInsensitiveLess comp(locale);

  std::map<std::string, int, CaseInsensitiveLess> test1;
  test1["foo"] = 1;
  BOOST_CHECK_EQUAL(1, (int)test1.count("foo"));
  BOOST_CHECK_EQUAL(1, (int)test1.count("Foo"));
  BOOST_CHECK_EQUAL(1, (int)test1.count("fOO"));
  BOOST_CHECK_EQUAL(0, (int)test1.count("bar"));

  std::map<std::string, int, CaseInsensitiveLess> test2(comp);
  test2["äää"] = 1;
  BOOST_CHECK_EQUAL(1, (int)test2.count("äää"));
  //// FIXME: ä-t ja ö-t eivät toimi kunnolla vielä
  // BOOST_CHECK_EQUAL(1, (int)test2.count("Äää"));
  // BOOST_CHECK_EQUAL(1, (int)test2.count("äÄä"));
  BOOST_CHECK_EQUAL(0, (int)test2.count("ööö"));
}
