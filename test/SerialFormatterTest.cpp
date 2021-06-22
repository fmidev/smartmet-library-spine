// ======================================================================
/*!
 * \file
 * \brief Regression tests for class SerialFormatter
 */
// ======================================================================

#include "HTTP.h"
#include "SerialFormatter.h"
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
namespace SerialFormatterTest
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
      "a:4:{i:0;a:4:{s:4:\"col0\";s:2:\"00\";s:4:\"col1\";s:2:\"10\";s:4:\"col2\";s:2:\"20\";s:4:"
      "\"col3\";s:2:\"30\";}i:1;a:4:{s:4:\"col0\";s:2:\"01\";s:4:\"col1\";s:2:\"11\";s:4:\"col2\";"
      "s:2:\"21\";s:4:\"col3\";s:2:\"31\";}i:2;a:4:{s:4:\"col0\";s:2:\"02\";s:4:\"col1\";s:2:"
      "\"12\";s:4:\"col2\";s:2:\"22\";s:4:\"col3\";s:2:\"32\";}i:3;a:4:{s:4:\"col0\";s:2:\"03\";s:"
      "4:\"col1\";s:2:\"13\";s:4:\"col2\";s:2:\"23\";s:4:\"col3\";s:2:\"33\";}}";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::SerialFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

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
      tab.set(i, j, tostr(i) + tostr(j));
  tab.set(2, 0, "Helsinki");
  tab.set(2, 1, "Tampere");
  tab.set(2, 2, "Helsinki");
  tab.set(2, 3, "Tampere");

  const char* res =
      "a:2:{s:8:\"Helsinki\";a:2:{i:0;a:3:{s:4:\"col0\";s:2:\"00\";s:4:\"col1\";s:2:\"10\";s:4:"
      "\"col3\";s:2:\"30\";}i:1;a:3:{s:4:\"col0\";s:2:\"02\";s:4:\"col1\";s:2:\"12\";s:4:\"col3\";"
      "s:2:\"32\";}}s:7:\"Tampere\";a:2:{i:0;a:3:{s:4:\"col0\";s:2:\"01\";s:4:\"col1\";s:2:\"11\";"
      "s:4:\"col3\";s:2:\"31\";}i:1;a:3:{s:4:\"col0\";s:2:\"03\";s:4:\"col1\";s:2:\"13\";s:4:"
      "\"col3\";s:2:\"33\";}}}";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "col2");

  SmartMet::Spine::SerialFormatter fmt;
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
      "a:2:{s:8:\"Helsinki\";a:2:{s:4:\"aamu\";a:2:{s:4:\"col0\";s:2:\"00\";s:4:\"col1\";s:2:"
      "\"10\";}s:4:\"ilta\";a:2:{s:4:\"col0\";s:2:\"02\";s:4:\"col1\";s:2:\"12\";}}s:7:\"Tampere\";"
      "a:2:{s:4:\"aamu\";a:2:{s:4:\"col0\";s:2:\"01\";s:4:\"col1\";s:2:\"11\";}s:4:\"ilta\";a:2:{s:"
      "4:\"col0\";s:2:\"03\";s:4:\"col1\";s:2:\"13\";}}}";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("attributes", "col2,col3");

  SmartMet::Spine::SerialFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

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

  SmartMet::Spine::SerialFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != "a:0:{}")
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

}  // namespace SerialFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "SerialFormatter tester" << endl << "======================" << endl;
  SerialFormatterTest::tests t;
  return t.run();
}

// ======================================================================
