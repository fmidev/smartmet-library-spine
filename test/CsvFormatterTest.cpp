// ======================================================================
/*!
 * \file
 * \brief Regression tests for class CsvFormatter
 */
// ======================================================================

#include "CsvFormatter.h"
#include "HTTP.h"
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
namespace CsvFormatterTest
{
// ----------------------------------------------------------------------

void format()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names{"a", "b", "c", "d"};

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = R"("a","b","c","d"
0,"NaN","NaN","NaN"
"NaN","NaN","NaN",31
"NaN",12,"NaN","NaN"
)";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void missingtext()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names{"a", "b", "c", "d"};

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = R"("a","b","c","d"
0,"-","-","-"
"-","-","-",31
"-",12,"-","-"
)";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("missingtext", "-");

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

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
  SmartMet::Spine::TableFormatter::Names names{"a", "b", "c", "d"};
  SmartMet::Spine::HTTP::Request req;

  const char* res = R"("a","b","c","d"
)";

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

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
    TEST(missingtext);
    TEST(empty);
  }
};

}  // namespace CsvFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "CsvFormatter tester" << endl << "====================" << endl;
  CsvFormatterTest::tests t;
  return t.run();
}
