// ======================================================================
/*!
 * \file
 * \brief Regression tests for class DebugFormatter
 */
// ======================================================================

#include "DebugFormatter.h"
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
namespace DebugFormatterTest
{
// ----------------------------------------------------------------------

void basic()
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
      "<html><head><title>Debug mode output</title><style>table { border: 1px solid black; }td { "
      "border: 1px solid black; "
      "text-align:right;}</style></head><body><table><tr><th>col0</th><th>col1</th><th>col2</"
      "th><th>col3</th></tr><tr>\n<td>0</td><td>10</td><td>20</td><td>30</td></tr><tr>\n<td>1</"
      "td><td>11</td><td>21</td><td>31</td></tr><tr>\n<td>2</td><td>12</td><td>22</td><td>32</td></"
      "tr><tr>\n<td>3</td><td>13</td><td>23</td><td>33</td></tr></table></body></html>";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::DebugFormatter fmt;
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

  SmartMet::Spine::DebugFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() !=
      "<html><head><title>Debug mode output</title><style>table { border: 1px solid black; }td { "
      "border: 1px solid black; "
      "text-align:right;}</style></head><body><table><tr></tr></table></body></html>")
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
    TEST(basic);
    TEST(empty);
  }
};

}  // namespace DebugFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "DebugFormatter tester" << endl << "=====================" << endl;
  DebugFormatterTest::tests t;
  return t.run();
}

// ======================================================================
