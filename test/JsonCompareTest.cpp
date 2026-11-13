// The JSON comparison of smartmet-plugin-test. The code is part of the
// application, not of the library, hence it is included here.
#include "../app/JsonCompare.cpp"
#include <boost/test/included/unit_test.hpp>
#include <cstring>

using namespace boost::unit_test;

namespace jc = SmartMet::Spine::JsonCompare;

namespace
{
const double default_tolerance = 1e-9;

// Empty if equal, otherwise the description of the first difference
std::string diff(const std::string& expected,
                 const std::string& result,
                 double tol = default_tolerance)
{
  const auto e = jc::parse(expected);
  const auto r = jc::parse(result);
  BOOST_REQUIRE(e);
  BOOST_REQUIRE(r);
  return jc::compare(*e, *r, tol).value_or("");
}
}  // namespace

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "JSON comparison tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_SUITE(json_compare_tests)

BOOST_AUTO_TEST_CASE(parse_valid)
{
  BOOST_TEST_MESSAGE("+ [JsonCompare] Parsing valid JSON");
  for (const char* text : {"null",
                           " true ",
                           "-0.5e+10",
                           "\"a\\\"b\\u00e4\"",
                           "[]",
                           "{}",
                           "[1, [2, {\"a\": null}], \"x\"]",
                           "{\"a\": {\"b\": [1.5, -2]}, \"c\": \"ä\"}\n"})
    BOOST_CHECK_MESSAGE(jc::parse(text), text);
}

BOOST_AUTO_TEST_CASE(parse_invalid)
{
  BOOST_TEST_MESSAGE("+ [JsonCompare] Rejecting invalid JSON");
  for (const char* text : {"",
                           "nan",
                           "01",
                           "1.",
                           ".5",
                           "1e",
                           "[1,]",
                           "{\"a\" 1}",
                           "{a: 1}",
                           "\"abc",
                           "\"\\x\"",
                           "[1] 2",
                           "Error: no data",
                           "{\"a\": tru}"})
    BOOST_CHECK_MESSAGE(!jc::parse(text), text);

  BOOST_CHECK(!jc::parse(std::string(2000, '[') + std::string(2000, ']')));
}

BOOST_AUTO_TEST_CASE(pretty)
{
  BOOST_TEST_MESSAGE("+ [JsonCompare] Pretty printing");
  const auto node = jc::parse(R"({"b":[1,2.50,{}],"a":"x y","c":[],"d":{"e":null}})");
  BOOST_REQUIRE(node);
  BOOST_CHECK_EQUAL(jc::pretty(*node),
                    "{\n"
                    "  \"b\": [\n"
                    "    1,\n"
                    "    2.50,\n"
                    "    {}\n"
                    "  ],\n"
                    "  \"a\": \"x y\",\n"
                    "  \"c\": [],\n"
                    "  \"d\": {\n"
                    "    \"e\": null\n"
                    "  }\n"
                    "}\n");
}

BOOST_AUTO_TEST_CASE(numbers)
{
  BOOST_TEST_MESSAGE("+ [JsonCompare] Comparing numbers");
  BOOST_CHECK(jc::numbers_equal("12.3", "12.300000000000001", default_tolerance));
  BOOST_CHECK(jc::numbers_equal("1.5", "1.50", default_tolerance));
  BOOST_CHECK(jc::numbers_equal("1", "1.0", default_tolerance));
  BOOST_CHECK(jc::numbers_equal("1e3", "1000", default_tolerance));
  BOOST_CHECK(jc::numbers_equal("0.1", "0.1000000001", default_tolerance));
  BOOST_CHECK(!jc::numbers_equal("0.1", "0.100000002", default_tolerance));
  // Relative for large numbers
  BOOST_CHECK(jc::numbers_equal("6000000.1", "6000000.1001", default_tolerance));
  BOOST_CHECK(!jc::numbers_equal("6000000.1", "6000000.11", default_tolerance));
  // Integers must match exactly
  BOOST_CHECK(!jc::numbers_equal("100000000000", "100000000001", default_tolerance));
  // Zero default_tolerance
  BOOST_CHECK(!jc::numbers_equal("12.3", "12.300000000000004", 0));
  BOOST_CHECK(jc::numbers_equal("12.3", "12.30", 0));
}

BOOST_AUTO_TEST_CASE(compare)
{
  BOOST_TEST_MESSAGE("+ [JsonCompare] Comparing documents");

  BOOST_CHECK_EQUAL(
      diff(R"({"a": [1, 2.5]})", "{\n  \"a\": [\n    1,\n    2.5000000000001\n  ]\n}"), "");

  BOOST_CHECK_EQUAL(diff(R"({"a": [1, 2.5]})", R"({"a": [1, 2.6]})"),
                    "$.a[1]: expected 2.5, got 2.6");

  // Strings are not numbers
  BOOST_CHECK_EQUAL(diff(R"(["2.5"])", R"(["2.5000000000001"])"),
                    "$[0]: expected \"2.5\", got \"2.5000000000001\"");

  // Strings are compared by value
  BOOST_CHECK_EQUAL(diff(R"({"\u0061": "\"\u00e4\/\ud83d\ude00\n"})", "{\"a\": \"\\\"ä/😀\\n\"}"),
                    "");

  BOOST_CHECK_EQUAL(diff(R"([2.5])", R"(["2.5"])"),
                    "$[0]: expected number 2.5, got string \"2.5\"");

  BOOST_CHECK_EQUAL(diff(R"([1, 2])", R"([1, 2, 3])"), "$: expected array of size 2, got size 3");

  // Member order matters
  BOOST_CHECK_EQUAL(diff(R"({"a": 1, "b": 2})", R"({"b": 2, "a": 1})"),
                    "$: expected member \"a\", got \"b\"");

  BOOST_CHECK_EQUAL(diff(R"({"a": 1, "b": 2})", R"({"a": 1})"), "$: missing member \"b\"");

  BOOST_CHECK_EQUAL(diff(R"({"a": 1})", R"({"a": 1, "b": 2})"), "$: unexpected member \"b\"");

  BOOST_CHECK_EQUAL(diff(R"({"x-y": {"1a": [null]}})", R"({"x-y": {"1a": [false]}})"),
                    "$[\"x-y\"][\"1a\"][0]: expected null null, got boolean false");
}

BOOST_AUTO_TEST_SUITE_END()
