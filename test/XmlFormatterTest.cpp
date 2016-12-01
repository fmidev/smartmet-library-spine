// ======================================================================
/*!
 * \file
 * \brief Regression tests for class XmlFormatter
 */
// ======================================================================

#include "XmlFormatter.h"
#include "Table.h"
#include "TableFormatterOptions.h"
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
namespace XmlFormatterTest
{
// ----------------------------------------------------------------------

void attributestyle()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<result>\n<row col0=\"0\" col1=\"nan\" "
      "col2=\"nan\" col3=\"nan\"/>\n<row col0=\"nan\" col1=\"nan\" col2=\"nan\" "
      "col3=\"31\"/>\n<row col0=\"nan\" col1=\"12\" col2=\"nan\" col3=\"nan\"/>\n</result>\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("xmlstyle", "attributes");

  SmartMet::Spine::XmlFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result: " + out.str());

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void tagstyle()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "?>\n<result>\n<row>\n<col0>0</col0>\n<col1>nan</col1>\n<col2>nan</col2>\n<col3>nan</"
      "col3>\n</row>\n<row>\n<col0>nan</col0>\n<col1>nan</col1>\n<col2>nan</col2>\n<col3>31</"
      "col3>\n</row>\n<row>\n<col0>nan</col0>\n<col1>12</col1>\n<col2>nan</col2>\n<col3>nan</"
      "col3>\n</row>\n</result>\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("xmlstyle", "tags");

  SmartMet::Spine::XmlFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result: " + out.str());

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void mixedstyle()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<result>\n<row col1=\"nan\" "
      "col2=\"nan\">\n<col0>0</col0>\n<col3>nan</col3>\n</row>\n<row col1=\"nan\" "
      "col2=\"nan\">\n<col0>nan</col0>\n<col3>31</col3>\n</row>\n<row col1=\"12\" "
      "col2=\"nan\">\n<col0>nan</col0>\n<col3>nan</col3>\n</row>\n</result>\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("xmlstyle", "mixed");
  req.setParameter("attributes", "col2,col1");

  SmartMet::Spine::XmlFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result: " + out.str());

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void missingtext()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");
  names.push_back("col2");
  names.push_back("col3");

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<result>\n<row col0=\"0\" col1=\"-\" "
      "col2=\"-\" col3=\"-\"/>\n<row col0=\"-\" col1=\"-\" col2=\"-\" col3=\"31\"/>\n<row "
      "col0=\"-\" col1=\"12\" col2=\"-\" col3=\"-\"/>\n</result>\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("missingtext", "-");

  SmartMet::Spine::XmlFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result: " + out.str());

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

  SmartMet::Spine::XmlFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<result>\n</result>\n")
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
    TEST(attributestyle);
    TEST(tagstyle);
    TEST(mixedstyle);
    TEST(missingtext);
    TEST(empty);
  }
};

}  // namespace XmlFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "XmlFormatter tester" << endl << "===================" << endl;
  XmlFormatterTest::tests t;
  return t.run();
}

// ======================================================================
