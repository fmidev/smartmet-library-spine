// ======================================================================
/*!
 * \file
 * \brief Regression tests for class Table
 */
// ======================================================================

#include "Table.h"
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

//! Protection against conflicts with global functions
namespace TableTest
{
// ----------------------------------------------------------------------

void basic()
{
  SmartMet::Spine::Table tab;

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 3; j++)
      tab.set(i, j, tostr(10 * i + j));

  if (tab.empty())
    TEST_FAILED("Test table should not be empty");

  auto cols = tab.columns();
  if (cols.size() != 4)
    TEST_FAILED("Test table should have 4 columns, not " + tostr(cols.size()));

  auto rows = tab.rows();
  if (rows.size() != 3)
    TEST_FAILED("Test table should have 3 rows, not " + tostr(rows.size()));

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Test an empty table
 */
// ----------------------------------------------------------------------

void empty()
{
  SmartMet::Spine::Table tab;

  if (!tab.empty())
    TEST_FAILED("Test table should not be empty");

  auto cols = tab.columns();
  if (cols.size() != 0)
    TEST_FAILED("Test table should have 0 columns, not " + tostr(cols.size()));

  auto rows = tab.rows();
  if (rows.size() != 0)
    TEST_FAILED("Test table should have 0 rows, not " + tostr(rows.size()));

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

}  // namespace TableTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "Table tester" << endl << "============" << endl;
  TableTest::tests t;
  return t.run();
}

// ======================================================================
