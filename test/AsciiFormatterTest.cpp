// ======================================================================
/*!
 * \file
 * \brief Regression tests for class AsciiFormatter
 */
// ======================================================================

#include "AsciiFormatter.h"
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
namespace AsciiFormatterTest
{
// ----------------------------------------------------------------------

void format()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = "0 nan nan nan\nnan nan nan 31\nnan 12 nan nan\n";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::AsciiFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result:\n" + out.str());

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void separator()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = "0,nan,nan,nan\nnan,nan,nan,31\nnan,12,nan,nan\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("separator", ",");

  SmartMet::Spine::AsciiFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result:\n" + out.str());

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void missingtext()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = "0 - - -\n- - - 31\n- 12 - -\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("missingtext", "-");

  SmartMet::Spine::AsciiFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result:\n" + out.str());

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

  SmartMet::Spine::AsciiFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != "")
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
    TEST(format);
    TEST(separator);
    TEST(missingtext);
    TEST(empty);
  }
};

}  // namespace AsciiFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "AsciiFormatter tester" << endl << "=====================" << endl;
  AsciiFormatterTest::tests t;
  return t.run();
}

// ======================================================================
