#include <boost/test/included/unit_test.hpp>
#include "DataFilter.h"

using SmartMet::Spine::DataFilter;

using namespace boost::unit_test;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Cache filter tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_SUITE(Test_Cache_filter)

BOOST_AUTO_TEST_SUITE(Cache)

BOOST_AUTO_TEST_CASE(test_empty)
{
  DataFilter filter;
  BOOST_CHECK_EQUAL(filter.getSqlClause("name", "x"), std::string(""));
}
BOOST_AUTO_TEST_CASE(test_number)
{
  DataFilter filter;
  filter.setDataFilter("name", "123");
  BOOST_CHECK_EQUAL(filter.getSqlClause("name", "x"), std::string("(x = 123)"));
}
BOOST_AUTO_TEST_CASE(test_eq)
{
  DataFilter filter;
  filter.setDataFilter("name", "eq 123");
  BOOST_CHECK_EQUAL(filter.getSqlClause("name", "x"), std::string("(x = 123)"));
}
BOOST_AUTO_TEST_CASE(test_lt)
{
  DataFilter filter;
  filter.setDataFilter("name", "lt 123");
  BOOST_CHECK_EQUAL(filter.getSqlClause("name", "x"), std::string("(x < 123)"));
}
BOOST_AUTO_TEST_CASE(test_le)
{
  DataFilter filter;
  filter.setDataFilter("name", "le 123");
  BOOST_CHECK(filter.getSqlClause("name", "x") == "(x <= 123)");
}
BOOST_AUTO_TEST_CASE(test_gt)
{
  DataFilter filter;
  filter.setDataFilter("name", "gt 123");
  BOOST_CHECK_EQUAL(filter.getSqlClause("name", "x"), std::string("(x > 123)"));
}
BOOST_AUTO_TEST_CASE(test_ge)
{
  DataFilter filter;
  filter.setDataFilter("name", "ge 123");
  BOOST_CHECK_EQUAL(filter.getSqlClause("name", "x"), std::string("(x >= 123)"));
}
BOOST_AUTO_TEST_CASE(test_and)
{
  DataFilter filter;
  filter.setDataFilter("name", "ge 1 AND lt 9");
  BOOST_CHECK_EQUAL(filter.getSqlClause("name", "x"),std::string("(x < 9 AND x >= 1)"));
}
BOOST_AUTO_TEST_CASE(test_or)
{
  DataFilter filter;
  filter.setDataFilter("name", "lt 5 OR ge 10");
  BOOST_CHECK_EQUAL(filter.getSqlClause("name", "x"), std::string("(x < 5 OR x >= 10)"));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(valueOK)

BOOST_AUTO_TEST_CASE(test_empty)
{
  DataFilter filter;
  BOOST_CHECK(filter.valueOK("empty", 1));
  BOOST_CHECK(filter.valueOK("empty", 2));
  BOOST_CHECK(filter.valueOK("empty", 3));
}

BOOST_AUTO_TEST_CASE(test_123)
{
  DataFilter filter;
  filter.setDataFilter("123", "123");
  BOOST_CHECK(!filter.valueOK("123", 122));
  BOOST_CHECK(filter.valueOK("123", 123));
  BOOST_CHECK(!filter.valueOK("123", 124));
}

BOOST_AUTO_TEST_CASE(test_123_124)
{
  DataFilter filter;
  filter.setDataFilter("123,124", "123,124");
  BOOST_CHECK(!filter.valueOK("123,124", 122));
  BOOST_CHECK(filter.valueOK("123,124", 123));
  BOOST_CHECK(filter.valueOK("123,124", 124));
}

BOOST_AUTO_TEST_CASE(test_eq_123)
{
  DataFilter filter;
  filter.setDataFilter("eq 123", "eq 123");
  BOOST_CHECK(!filter.valueOK("eq 123", 122));
  BOOST_CHECK(filter.valueOK("eq 123", 123));
  BOOST_CHECK(!filter.valueOK("eq 123", 124));
}

BOOST_AUTO_TEST_CASE(test_lt_123)
{
  DataFilter filter;
  filter.setDataFilter("lt 123", "lt 123");
  BOOST_CHECK(filter.valueOK("lt 123", 122));
  BOOST_CHECK(!filter.valueOK("lt 123", 123));
  BOOST_CHECK(!filter.valueOK("lt 123", 124));
}

BOOST_AUTO_TEST_CASE(test_le_123)
{
  DataFilter filter;
  filter.setDataFilter("le 123", "le 123");
  BOOST_CHECK(filter.valueOK("le 123", 122));
  BOOST_CHECK(filter.valueOK("le 123", 123));
  BOOST_CHECK(!filter.valueOK("le 123", 124));
}

BOOST_AUTO_TEST_CASE(test_gt_123)
{
  DataFilter filter;
  filter.setDataFilter("gt 123", "gt 123");
  BOOST_CHECK(!filter.valueOK("gt 123", 122));
  BOOST_CHECK(!filter.valueOK("gt 123", 123));
  BOOST_CHECK(filter.valueOK("gt 123", 124));
}

BOOST_AUTO_TEST_CASE(test_ge_123)
{
  DataFilter filter;
  filter.setDataFilter("ge 123", "ge 123");
  BOOST_CHECK(!filter.valueOK("ge 123", 122));
  BOOST_CHECK(filter.valueOK("ge 123", 123));
  BOOST_CHECK(filter.valueOK("ge 123", 124));
}

BOOST_AUTO_TEST_CASE(test_ge_1_AND_lt_9)
{
  DataFilter filter;
  filter.setDataFilter("ge 1 AND lt 9", "ge 1 AND lt 9");
  BOOST_CHECK(!filter.valueOK("ge 1 AND lt 9", 0));
  BOOST_CHECK(filter.valueOK("ge 1 AND lt 9", 1));
  BOOST_CHECK(filter.valueOK("ge 1 AND lt 9", 2));
  BOOST_CHECK(filter.valueOK("ge 1 AND lt 9", 8));
  BOOST_CHECK(!filter.valueOK("ge 1 AND lt 9", 9));
  BOOST_CHECK(!filter.valueOK("ge 1 AND lt 9", 10));
}

BOOST_AUTO_TEST_CASE(test_lt_5_OR_ge_10)
{
  DataFilter filter;
  filter.setDataFilter("lt 5 OR ge 10", "lt 5 OR ge 10");
  BOOST_CHECK(filter.valueOK("lt 5 OR ge 10", 4));
  BOOST_CHECK(filter.valueOK("lt 5 OR ge 10", 10));
  BOOST_CHECK(filter.valueOK("lt 5 OR ge 10", 11));
  BOOST_CHECK(!filter.valueOK("lt 5 OR ge 10", 5));
  BOOST_CHECK(!filter.valueOK("lt 5 OR ge 10", 6));
  BOOST_CHECK(!filter.valueOK("lt 5 OR ge 10", 9));
}

BOOST_AUTO_TEST_CASE(test_1_3_ge_5_AND_lt_9_11)
{
  DataFilter filter;
  filter.setDataFilter("1,3,ge 5 AND lt 9,11", "1,3,ge 5 AND lt 9,11");
  BOOST_CHECK(!filter.valueOK("1,3,ge 5 AND lt 9,11", 0));
  BOOST_CHECK(filter.valueOK("1,3,ge 5 AND lt 9,11", 1));
  BOOST_CHECK(!filter.valueOK("1,3,ge 5 AND lt 9,11", 2));
  BOOST_CHECK(filter.valueOK("1,3,ge 5 AND lt 9,11", 3));
  BOOST_CHECK(!filter.valueOK("1,3,ge 5 AND lt 9,11", 4));
  BOOST_CHECK(filter.valueOK("1,3,ge 5 AND lt 9,11", 5));
  BOOST_CHECK(filter.valueOK("1,3,ge 5 AND lt 9,11", 6));
  BOOST_CHECK(filter.valueOK("1,3,ge 5 AND lt 9,11", 7));
  BOOST_CHECK(filter.valueOK("1,3,ge 5 AND lt 9,11", 8));
  BOOST_CHECK(!filter.valueOK("1,3,ge 5 AND lt 9,11", 9));
  BOOST_CHECK(!filter.valueOK("1,3,ge 5 AND lt 9,11", 10));
  BOOST_CHECK(filter.valueOK("1,3,ge 5 AND lt 9,11", 11));
  BOOST_CHECK(!filter.valueOK("1,3,ge 5 AND lt 9,11", 12));
  BOOST_CHECK(!filter.valueOK("1,3,ge 5 AND lt 9,11", 13));
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
