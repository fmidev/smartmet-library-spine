// ======================================================================
/*!
 * \file
 * \brief Regression tests for namespace ValueFormatter
 */
// ======================================================================

#include "ValueFormatter.h"
#include <regression/tframe.h>
#include <cmath>
#include <limits>
#include <sstream>

template <typename T>
std::string tostr(const T& theValue)
{
  std::ostringstream out;
  out << theValue;
  return out.str();
}

//! Protection against conflicts with global functions
namespace ValueFormatterTest
{
// ----------------------------------------------------------------------

void simple()
{
  SmartMet::Spine::HTTP::Request req;
  req.setParameter("floatfield", "none");
  SmartMet::Spine::ValueFormatter fmt(req);

  int precision = -1;

  std::string result;

  if ((result = fmt.format(1, precision)) != "1")
    TEST_FAILED("Formatting 1 failed: " + result);

  if ((result = fmt.format(1.23, precision)) != "1.23")
    TEST_FAILED("Formatting 1.23 failed: " + result);

  if ((result = fmt.format(-1.23, precision)) != "-1.23")
    TEST_FAILED("Formatting -1.23 failed: " + result);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void nan()
{
  std::string result;
  int precision = -1;

  const float nan = std::numeric_limits<float>::quiet_NaN();

  {
    SmartMet::Spine::HTTP::Request req;
    req.setParameter("floatfield", "none");
    SmartMet::Spine::ValueFormatter fmt(req);
    if ((result = fmt.format(nan, precision)) != "nan")
      TEST_FAILED("Failed to return 'nan' for NaN value, result = " + result);
  }

  {
    SmartMet::Spine::HTTP::Request req;
    req.setParameter("floatfield", "none");
    req.setParameter("missingtext", "NULL");
    SmartMet::Spine::ValueFormatter fmt(req);
    if ((result = fmt.format(nan, precision)) != "NULL")
      TEST_FAILED("Failed to return 'NULL' for NaN value, result = " + result);
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void width()
{
  SmartMet::Spine::HTTP::Request req;
  req.setParameter("floatfield", "none");
  req.setParameter("width", "6");
  int precision = -1;
  SmartMet::Spine::ValueFormatter fmt(req);
  std::string result;

  if ((result = fmt.format(1, precision)) != "     1")
    TEST_FAILED("Formatting 1 failed: " + result);

  if ((result = fmt.format(1.23, precision)) != "  1.23")
    TEST_FAILED("Formatting 1.23 failed: " + result);

  if ((result = fmt.format(-1.23, precision)) != " -1.23")
    TEST_FAILED("Formatting -1.23 failed: " + result);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void fillchar()
{
  SmartMet::Spine::HTTP::Request req;
  req.setParameter("floatfield", "none");
  req.setParameter("width", "6");
  req.setParameter("fill", "_");
  int precision = -1;
  SmartMet::Spine::ValueFormatter fmt(req);

  std::string result;

  if ((result = fmt.format(1, precision)) != "_____1")
    TEST_FAILED("Formatting 1 failed: " + result);

  if ((result = fmt.format(1.23, precision)) != "__1.23")
    TEST_FAILED("Formatting 1.23 failed: " + result);

  if ((result = fmt.format(-1.23, precision)) != "_-1.23")
    TEST_FAILED("Formatting -1.23 failed: " + result);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void adjustfield()
{
  std::string result;
  int precision = -1;

  {
    SmartMet::Spine::HTTP::Request req;
    req.setParameter("floatfield", "none");
    req.setParameter("width", "6");
    req.setParameter("adjustfield", "right");
    SmartMet::Spine::ValueFormatter fmt(req);

    if ((result = fmt.format(1, precision)) != "     1")
      TEST_FAILED("Formatting 1 failed: " + result);

    if ((result = fmt.format(1.23, precision)) != "  1.23")
      TEST_FAILED("Formatting 1.23 failed: " + result);

    if ((result = fmt.format(-1.23, precision)) != " -1.23")
      TEST_FAILED("Formatting -1.23 failed: " + result);
  }

  {
    SmartMet::Spine::HTTP::Request req;
    req.setParameter("floatfield", "none");
    req.setParameter("width", "6");
    req.setParameter("adjustfield", "left");
    SmartMet::Spine::ValueFormatter fmt(req);

    if ((result = fmt.format(1, precision)) != "1     ")
      TEST_FAILED("Formatting 1 failed: " + result);

    if ((result = fmt.format(1.23, precision)) != "1.23  ")
      TEST_FAILED("Formatting 1.23 failed: " + result);

    if ((result = fmt.format(-1.23, precision)) != "-1.23 ")
      TEST_FAILED("Formatting -1.23 failed: " + result);
  }

  {
    SmartMet::Spine::HTTP::Request req;
    req.setParameter("floatfield", "none");
    req.setParameter("width", "6");
    req.setParameter("adjustfield", "internal");

    SmartMet::Spine::ValueFormatter fmt(req);

    if ((result = fmt.format(1, precision)) != " 00001")
      TEST_FAILED("Formatting 1 failed: '" + result + "'");

    if ((result = fmt.format(1.23, precision)) != " 01.23")
      TEST_FAILED("Formatting 1.23 failed: '" + result + "'");

    if ((result = fmt.format(-1.23, precision)) != "-01.23")
      TEST_FAILED("Formatting -1.23 failed: '" + result + "'");
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void floatfield()
{
  std::string result;
  int precision = -1;

  {
    SmartMet::Spine::HTTP::Request req;
    req.setParameter("floatfield", "fixed");
    SmartMet::Spine::ValueFormatter fmt(req);

    if ((result = fmt.format(1, precision)) != "1")
      TEST_FAILED("Formatting 1 failed: " + result);

    if ((result = fmt.format(1.23, precision)) != "1.23")
      TEST_FAILED("Formatting 1.23 failed: " + result);

    if ((result = fmt.format(-1.23, precision)) != "-1.23")
      TEST_FAILED("Formatting -1.23 failed: " + result);
  }

  {
    SmartMet::Spine::HTTP::Request req;
    req.setParameter("floatfield", "scientific");
    SmartMet::Spine::ValueFormatter fmt(req);

    if ((result = fmt.format(1, precision)) != "1")
      TEST_FAILED("Formatting 1 failed: " + result);

    if ((result = fmt.format(1.23, precision)) != "1.23")
      TEST_FAILED("Formatting 1.23 failed: " + result);

    if ((result = fmt.format(-1.23, precision)) != "-1.23")
      TEST_FAILED("Formatting -1.23 failed: " + result);

    if ((result = fmt.format(2.3e-2, 1)) != "2.3e-02")
      TEST_FAILED("Formatting 2.3e-2 failed: " + result);
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void precision()
{
  std::string result;

  {
    SmartMet::Spine::HTTP::Request req;
    req.setParameter("floatfield", "none");
    int precision = 2;
    SmartMet::Spine::ValueFormatter fmt(req);

    if ((result = fmt.format(1, precision)) != "1")
      TEST_FAILED("Formatting 1 failed: " + result);

    if ((result = fmt.format(1.23, precision)) != "1.2")
      TEST_FAILED("Formatting 1.23 failed: " + result);

    if ((result = fmt.format(-1.23, precision)) != "-1.2")
      TEST_FAILED("Formatting -1.23 failed: " + result);

    if ((result = fmt.format(1.25, precision)) != "1.3")
      TEST_FAILED("Formatting 1.25 failed: " + result);

    if ((result = fmt.format(-1.25, precision)) != "-1.3")
      TEST_FAILED("Formatting -1.25 failed: " + result);

    if ((result = fmt.format(0.10001, precision)) != "0.1")
      TEST_FAILED("Formatting 0.10001 failed: " + result);
  }

  {
    SmartMet::Spine::HTTP::Request req;
    int precision = 2;
    req.setParameter("floatfield", "fixed");
    SmartMet::Spine::ValueFormatter fmt(req);

    if ((result = fmt.format(1, precision)) != "1.00")
      TEST_FAILED("Formatting 1 failed: " + result);

    if ((result = fmt.format(1.23, precision)) != "1.23")
      TEST_FAILED("Formatting 1.23 failed: " + result);

    if ((result = fmt.format(-1.23, precision)) != "-1.23")
      TEST_FAILED("Formatting -1.23 failed: " + result);

    if ((result = fmt.format(5.555, precision)) != "5.56")
      TEST_FAILED("Formatting 5.555 failed: " + result);

    if ((result = fmt.format(5.599, precision)) != "5.60")
      TEST_FAILED("Formatting 5.599 failed: " + result);
  }

  {
    SmartMet::Spine::HTTP::Request req;
    int precision = 2;
    req.setParameter("floatfield", "scientific");
    SmartMet::Spine::ValueFormatter fmt(req);

    if ((result = fmt.format(1, precision)) != "1.00e+00")
      TEST_FAILED("Formatting 1 failed: " + result);

    if ((result = fmt.format(1.23, precision)) != "1.23e+00")
      TEST_FAILED("Formatting 1.23 failed: " + result);

    if ((result = fmt.format(-1.23, precision)) != "-1.23e+00")
      TEST_FAILED("Formatting -1.23 failed: " + result);

    if ((result = fmt.format(5.555, precision)) != "5.55e+00")
      TEST_FAILED("Formatting 5.555 failed: " + result);
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void showpos()
{
  std::string result;

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("floatfield", "none");
  req.setParameter("showpos", "1");
  int precision = -1;
  SmartMet::Spine::ValueFormatter fmt(req);

  if ((result = fmt.format(1, precision)) != "+1")
    TEST_FAILED("Formatting 1 failed: " + result);

  if ((result = fmt.format(1.23, precision)) != "+1.23")
    TEST_FAILED("Formatting 1.23 failed: " + result);

  if ((result = fmt.format(-1.23, precision)) != "-1.23")
    TEST_FAILED("Formatting -1.23 failed: " + result);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void uppercase()
{
  std::string result;

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("uppercase", "1");
  req.setParameter("floatfield", "scientific");
  int precision = -1;
  SmartMet::Spine::ValueFormatter fmt(req);

  if ((result = fmt.format(1, precision)) != "1")
    TEST_FAILED("Formatting 1 failed: " + result);

  if ((result = fmt.format(1.23, precision)) != "1.23")
    TEST_FAILED("Formatting 1.23 failed: " + result);

  if ((result = fmt.format(-1.23, precision)) != "-1.23")
    TEST_FAILED("Formatting -1.23 failed: " + result);

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
    TEST(simple);
    TEST(nan);
    TEST(width);
    TEST(fillchar);
    TEST(precision);
    TEST(adjustfield);
    TEST(showpos);
    TEST(uppercase);
    TEST(floatfield);
  }
};

}  // namespace ValueFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "ValueFormatter tester" << endl << "=====================" << endl;
  ValueFormatterTest::tests t;
  return t.run();
}

// ======================================================================
