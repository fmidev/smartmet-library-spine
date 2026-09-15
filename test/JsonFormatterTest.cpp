// ======================================================================
/*!
 * \file
 * \brief Regression tests for class JsonFormatter
 */
// ======================================================================

#include "HTTP.h"
#include "JsonFormatter.h"
#include "Table.h"
#include "TableFormatterOptions.h"
#include <json/json.h>
#include <regression/tframe.h>
#include <cmath>
#include <utility>
#include <vector>

template <typename T>
std::string tostr(const T& theValue)
{
  std::ostringstream out;
  out << theValue;
  return out.str();
}

SmartMet::Spine::TableFormatterOptions config;

//! Protection against conflicts with global functions
namespace JsonFormatterTest
{
// ----------------------------------------------------------------------

void noattributes()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      tab.set(i, j, tostr(10 * i + j));

  const char* res =
      "[{\"col0\":0,\"col1\":10,\"col2\":20,\"col3\":30},{\"col0\":1,\"col1\":11,\"col2\":21,"
      "\"col3\":31},{\"col0\":2,\"col1\":12,\"col2\":22,\"col3\":32},{\"col0\":3,\"col1\":13,"
      "\"col2\":23,\"col3\":33}]";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::JsonFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "\nexpected:\n" + res);

  TEST_PASSED();
}

void noattributes_names_from_table()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");
  tab.setNames(names);

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      tab.set(i, j, tostr(10 * i + j));

  const char* res =
      "[{\"col0\":0,\"col1\":10,\"col2\":20,\"col3\":30},{\"col0\":1,\"col1\":11,\"col2\":21,"
      "\"col3\":31},{\"col0\":2,\"col1\":12,\"col2\":22,\"col3\":32},{\"col0\":3,\"col1\":13,"
      "\"col2\":23,\"col3\":33}]";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::JsonFormatter fmt;
  auto out = fmt.format(tab, {}, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "\nexpected:\n" + res);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void oneattribute()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      tab.set(i, j, tostr(10 * i + j));
  tab.set(2, 0, "Helsinki");
  tab.set(2, 1, "Tampere");
  tab.set(2, 2, "Helsinki");
  tab.set(2, 3, "Tampere");

  const char* res =
      "{\"Helsinki\":[{\"col0\":0,\"col1\":10,\"col3\":30},{\"col0\":2,\"col1\":12,\"col3\":32}],"
      "\"Tampere\":[{\"col0\":1,\"col1\":11,\"col3\":31},{\"col0\":3,\"col1\":13,\"col3\":33}]}";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "col2");

  SmartMet::Spine::JsonFormatter fmt;

  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "\nexpected:\n" + res);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void twoattributes()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      tab.set(i, j, tostr(i) + tostr(j));
  tab.set(2, 0, "Helsinki");
  tab.set(2, 1, "Tampere");
  tab.set(2, 2, "Helsinki");
  tab.set(2, 3, "Tampere");
  tab.set(3, 0, "aamu");
  tab.set(3, 1, "aamu");
  tab.set(3, 2, "ilta");
  tab.set(3, 3, "ilta");

  const char* res =
      "{\"Helsinki\":{\"aamu\":[{\"col0\":0,\"col1\":10}],\"ilta\":[{\"col0\":2,\"col1\":12}]},"
      "\"Tampere\":{\"aamu\":[{\"col0\":1,\"col1\":11}],\"ilta\":[{\"col0\":3,\"col1\":13}]}}";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "col2,col3");

  SmartMet::Spine::JsonFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "\nexpected:\n" + res);

  TEST_PASSED();
}

void twoattributes_names_from_table()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");
  tab.setNames(names);

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      tab.set(i, j, tostr(i) + tostr(j));
  tab.set(2, 0, "Helsinki");
  tab.set(2, 1, "Tampere");
  tab.set(2, 2, "Helsinki");
  tab.set(2, 3, "Tampere");
  tab.set(3, 0, "aamu");
  tab.set(3, 1, "aamu");
  tab.set(3, 2, "ilta");
  tab.set(3, 3, "ilta");

  const char* res =
      "{\"Helsinki\":{\"aamu\":[{\"col0\":0,\"col1\":10}],\"ilta\":[{\"col0\":2,\"col1\":12}]},"
      "\"Tampere\":{\"aamu\":[{\"col0\":1,\"col1\":11}],\"ilta\":[{\"col0\":3,\"col1\":13}]}}";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "col2,col3");

  SmartMet::Spine::JsonFormatter fmt;
  auto out = fmt.format(tab, {}, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "\nexpected:\n" + res);

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Test formatting an empty table
 */
// ----------------------------------------------------------------------

void empty()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::JsonFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != "[]")
    TEST_FAILED("Incorrect result:\n" + out);

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Test escaping
 */
// ----------------------------------------------------------------------

void escaping()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("newline");
  names.push_back("control chars");
  names.push_back("quotation mark");
  names.push_back("backslash");

  tab.set(0, 0, "abc\ndef");
  tab.set(1, 0, "\x01\x02\x03...\x1e\x1f\x20\x21\x22\x23\x24\x25\x25\x27");
  tab.set(2, 0, "\"\"\"");
  tab.set(3, 0, "\\\\\\");

  const char* res =
      R"([{"newline":"abc\u000adef","control chars":"\u0001\u0002\u0003...\u001e\u001f !\u0022#$%%'","quotation mark":"\u0022\u0022\u0022","backslash":"\u005c\u005c\u005c"}])";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::JsonFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "\nexpected:\n" + res);

  TEST_PASSED();
}

void no_names_1()
{
  SmartMet::Spine::Table tab;
  tab.set(0, 0, "abc\ndef");
  tab.set(1, 0, "\x01\x02\x03...\x1e\x1f\x20\x21\x22\x23\x24\x25\x25\x27");
  tab.set(2, 0, "\"\"\"");
  tab.set(3, 0, "\\\\\\");
  SmartMet::Spine::JsonFormatter fmt;
  SmartMet::Spine::HTTP::Request req;
  try
  {
    fmt.format(tab, {}, req, config);
    TEST_FAILED("Expected exception not thrown");
  }
  catch (const std::exception& e)
  {
    TEST_PASSED();
  }
}

void no_names_2()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  tab.set(0, 0, "abc\ndef");
  tab.set(1, 0, "\x01\x02\x03...\x1e\x1f\x20\x21\x22\x23\x24\x25\x25\x27");
  tab.set(2, 0, "\"\"\"");
  tab.set(3, 0, "\\\\\\");
  SmartMet::Spine::JsonFormatter fmt;
  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "foo,bar");
  try
  {
    fmt.format(tab, {}, req, config);
    TEST_FAILED("Expected exception not thrown");
  }
  catch (const std::exception& e)
  {
    TEST_PASSED();
  }
}

void no_names_3()
{
  // Variant when there is only colunn 0 in the table and name vector is empty
  // requires special check - therefore test here
  SmartMet::Spine::Table tab;
  tab.set(0, 0, "abc\ndef");
  SmartMet::Spine::JsonFormatter fmt;
  SmartMet::Spine::HTTP::Request req;
  try
  {
    fmt.format(tab, {}, req, config);
    TEST_FAILED("Expected exception not thrown");
  }
  catch (const std::exception& e)
  {
    TEST_PASSED();
  }
}

void not_enough_names_1()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names = {"foo", "bar"};
  tab.setNames(names);

  tab.set(0, 0, "abc\ndef");
  tab.set(1, 0, "\x01\x02\x03...\x1e\x1f\x20\x21\x22\x23\x24\x25\x25\x27");
  tab.set(2, 0, "\"\"\"");
  tab.set(3, 0, "\\\\\\");
  SmartMet::Spine::JsonFormatter fmt;
  SmartMet::Spine::HTTP::Request req;
  try
  {
    fmt.format(tab, {}, req, config);
    TEST_FAILED("Expected exception not thrown");
  }
  catch (const std::exception& e)
  {
    TEST_PASSED();
  }
}

void not_enough_names_2()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names = {"foo", "bar"};
  tab.setNames(names);

  tab.set(0, 0, "abc\ndef");
  tab.set(1, 0, "\x01\x02\x03...\x1e\x1f\x20\x21\x22\x23\x24\x25\x25\x27");
  tab.set(2, 0, "\"\"\"");
  tab.set(3, 0, "\\\\\\");
  SmartMet::Spine::JsonFormatter fmt;
  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "foo,bar");
  try
  {
    fmt.format(tab, {}, req, config);
    TEST_FAILED("Expected exception not thrown");
  }
  catch (const std::exception& e)
  {
    TEST_PASSED();
  }
}

void number_detection_1()
{
  SmartMet::Spine::TableFormatter::Names names = {"foo"};

  const std::vector<std::string> input = {
      "123",    "45.67",    "abc",       "Nan", "Inf",  "-Inf", "+Inf", "123x",  "01",
      ".12345", ".12345E4", "12345E-4",  "+1",  "nan",  "NaN",  "NAN",  "-nan",  "nan(1)",
      "inf",    "infinity", "-infinity", "INF", "0",    "-0",   "000",  "-007",  "00.5",
      "-.5",    "+.5",      "5.",        "-5.", "1e+5", "1E5",  "1e",   "1.2.3", "0x10",
      ".",      "-",        "+",         " 1",  "1 ",   "--1",  "1,2"};

  int num_errors = 0;
  for (std::size_t i = 0; i < input.size(); ++i)
  {
    SmartMet::Spine::Table tab;
    tab.setNames(names);
    tab.set(0, i, input[i]);
    SmartMet::Spine::JsonFormatter fmt;
    SmartMet::Spine::HTTP::Request req;
    const auto out = fmt.format(tab, {}, req, config);
    std::shared_ptr<Json::Value> root;
    try
    {
      root = std::make_shared<Json::Value>();
      std::istringstream(out) >> *root;
      //std::cout << "Formatted JSON output for input[" << i << "]:\n" << *root << std::endl;
    }
    catch (const std::exception& e)
    {
      std::cerr << "Exception caught while parsing JSON for input '" << input[i]
                << "': " << e.what() << '\n'
                << "  in: " << out << std::endl;
      ++num_errors;
    }
  }

  if (num_errors != 0)
    TEST_FAILED("Number of JSON parsing errors: " + std::to_string(num_errors));

  TEST_PASSED();
}

void number_normalization()
{
  // Values which are formatted as JSON numbers. Note that Boost.Spirit accepts
  // several forms which are not valid JSON and which hence must be normalized.
  const std::vector<std::pair<std::string, std::string>> numbers = {
      {"123", "123"},
      {"45.67", "45.67"},
      {"-45.67", "-45.67"},
      {"-1", "-1"},
      {"+1", "1"},  // JSON does not allow a leading plus sign
      {"01", "1"},  // JSON does not allow leading zeroes
      {"-007", "-7"},
      {"000", "0"},
      {"00.5", "0.5"},
      {"0", "0"},
      {"-0", "-0"},
      {"0.5", "0.5"},
      {".12345", "0.12345"},  // JSON requires an integer part
      {"-.5", "-0.5"},
      {"+.5", "0.5"},
      {"5.", "5"},  // JSON does not allow a trailing decimal point
      {"-5.", "-5"},
      {".12345E4", "0.12345E4"},
      {"12345E-4", "12345E-4"},
      {"1e+5", "1e+5"},
      {"1E5", "1E5"},
      {"5.e3", "5e3"},
      {"0100.50", "100.50"}};

  // Values which are not numbers and are hence formatted as JSON strings. In
  // particular the special values NaN and Inf accepted by Boost.Spirit have no
  // JSON representation, and neither do station names looking like them.
  const std::vector<std::string> strings = {"abc",
                                            "Nan",  // station in Thailand
                                            "NAN",   "-nan",     "nan(1)",    "Inf", "-Inf", "+Inf",
                                            "inf",   "infinity", "-infinity", "INF", "123x", "1e",
                                            "1.2.3", "0x10",     ".",         "-",   "+",    " 1",
                                            "1 ",    "--1",      "1,2"};

  // Values reported as missing values
  const std::vector<std::string> nulls = {"", "nan", "NaN"};

  std::vector<std::pair<std::string, std::string>> expected = numbers;
  for (const auto& value : strings)
    expected.emplace_back(value, "\"" + value + "\"");
  for (const auto& value : nulls)
    expected.emplace_back(value, "null");

  const SmartMet::Spine::TableFormatter::Names names = {"foo"};

  std::string errors;
  for (const auto& test : expected)
  {
    SmartMet::Spine::Table tab;
    tab.setNames(names);
    tab.set(0, 0, test.first);

    SmartMet::Spine::JsonFormatter fmt;
    SmartMet::Spine::HTTP::Request req;
    const auto out = fmt.format(tab, {}, req, config);
    const std::string res = "[{\"foo\":" + test.second + "}]";

    if (out != res)
      errors += "\n\tinput '" + test.first + "': got " + out + ", expected " + res;
  }

  if (!errors.empty())
    TEST_FAILED("Incorrect results:" + errors);

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * The actual test suite
 */
// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(noattributes);
    TEST(noattributes_names_from_table);
    TEST(oneattribute);
    TEST(twoattributes);
    TEST(twoattributes_names_from_table);
    TEST(empty);
    TEST(escaping);
    TEST(no_names_1);
    TEST(no_names_2);
    TEST(no_names_3);
    TEST(not_enough_names_1);
    TEST(not_enough_names_2);
    TEST(number_detection_1);
    TEST(number_normalization);
    // TEST(missingtext);
  }
};

}  // namespace JsonFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "JsonFormatter tester" << endl << "====================" << endl;
  JsonFormatterTest::tests t;
  return t.run();
}

// ======================================================================
