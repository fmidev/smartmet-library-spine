// ======================================================================
/*!
 * \file
 * \brief Regression tests for class JsonFormatter
 */
// ======================================================================

#include "JsonFormatter.h"
#include "Table.h"
#include "TableFormatterOptions.h"
#include "HTTP.h"
#include <regression/tframe.h>
#include <sstream>
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
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result:\n" + out.str() + "\nexpected:\n" + res);

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
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result:\n" + out.str() + "\nexpected:\n" + res);

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
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result:\n" + out.str() + "\nexpected:\n" + res);

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
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != "[]")
    TEST_FAILED("Incorrect result:\n" + out.str());

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
    TEST(oneattribute);
    TEST(twoattributes);
    TEST(empty);
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
