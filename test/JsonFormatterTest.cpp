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
#include <regression/tframe.h>
#include <cmath>

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
      "{\"Helsinki\":{\"aamu\":[{\"col0\":00,\"col1\":10}],\"ilta\":[{\"col0\":02,\"col1\":12}]},"
      "\"Tampere\":{\"aamu\":[{\"col0\":01,\"col1\":11}],\"ilta\":[{\"col0\":03,\"col1\":13}]}}";

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
      "{\"Helsinki\":{\"aamu\":[{\"col0\":00,\"col1\":10}],\"ilta\":[{\"col0\":02,\"col1\":12}]},"
      "\"Tampere\":{\"aamu\":[{\"col0\":01,\"col1\":11}],\"ilta\":[{\"col0\":03,\"col1\":13}]}}";

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
  catch(const std::exception& e)
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
  catch(const std::exception& e)
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
  catch(const std::exception& e)
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
  catch(const std::exception& e)
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
  catch(const std::exception& e)
  {
    TEST_PASSED();
  }
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
    TEST(oneattribute);
    TEST(twoattributes);
    TEST(empty);
    TEST(escaping);
    TEST(no_names_1);
    TEST(no_names_2);
    TEST(not_enough_names_1);
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
