// ======================================================================
/*!
 * \file
 * \brief Regression tests for class PhpFormatter
 */
// ======================================================================

#include "HTTP.h"
#include "PhpFormatter.h"
#include "Table.h"
#include "TableFormatterOptions.h"
#include <regression/tframe.h>
#include <cmath>
#include <sstream>

template <typename T>
std::string tostr(const T& theValue)
{
  std::ostringstream out;
  out << theValue;
  return out.str();
}

SmartMet::Spine::TableFormatterOptions config;

//! Protection against conflicts with global functions
namespace PhpFormatterTest
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
      tab.set(i, j, tostr(i) + tostr(j));

  const char* res =
      "array(\narray(\n\"col0\" => \"00\",\n\"col1\" => \"10\",\n\"col2\" => \"20\",\n\"col3\" => "
      "\"30\"\n),\narray(\n\"col0\" => \"01\",\n\"col1\" => \"11\",\n\"col2\" => \"21\",\n\"col3\" "
      "=> \"31\"\n),\narray(\n\"col0\" => \"02\",\n\"col1\" => \"12\",\n\"col2\" => "
      "\"22\",\n\"col3\" => \"32\"\n),\narray(\n\"col0\" => \"03\",\n\"col1\" => \"13\",\n\"col2\" "
      "=> \"23\",\n\"col3\" => \"33\"\n));\n";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::PhpFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result: " + out);

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
      tab.set(i, j, tostr(i) + tostr(j));
  tab.set(2, 0, "Helsinki");
  tab.set(2, 1, "Tampere");
  tab.set(2, 2, "Helsinki");
  tab.set(2, 3, "Tampere");

  const char* res =
      "array(\n\"Helsinki\" => array(\narray(\"col0\" => \"00\",\n\"col1\" => \"10\",\n\"col3\" => "
      "\"30\"),\narray(\n\"col0\" => \"02\",\n\"col1\" => \"12\",\n\"col3\" => "
      "\"32\")),\n\"Tampere\" => array(\narray(\"col0\" => \"01\",\n\"col1\" => \"11\",\n\"col3\" "
      "=> \"31\"),\narray(\n\"col0\" => \"03\",\n\"col1\" => \"13\",\n\"col3\" => \"33\")));\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "col2");

  SmartMet::Spine::PhpFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result: " + out);

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
      "array(\n\"Helsinki\" => array(\n\"aamu\" => array(\n\"col0\" => \"00\",\n\"col1\" => "
      "\"10\"),\n\"ilta\" => array(\n\"col0\" => \"02\",\n\"col1\" => \"12\")),\n\"Tampere\" => "
      "array(\n\"aamu\" => array(\n\"col0\" => \"01\",\n\"col1\" => \"11\"),\n\"ilta\" => "
      "array(\n\"col0\" => \"03\",\n\"col1\" => \"13\")));\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "col2,col3");

  SmartMet::Spine::PhpFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result: " + out);

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

  SmartMet::Spine::PhpFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != "array(\n);\n")
    TEST_FAILED("Incorrect result:\n" + out);

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

}  // namespace PhpFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "PhpFormatter tester" << endl << "===================" << endl;
  PhpFormatterTest::tests t;
  return t.run();
}

// ======================================================================
