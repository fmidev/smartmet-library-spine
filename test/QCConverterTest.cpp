// ======================================================================
/*!
 * \file
 * \brief Tests for class QCConverter
 */
// ======================================================================

#include "QCConverter.h"
#include <regression/tframe.h>
#include <cmath>
#include <sstream>

//! Protection against conflicts with global functions
namespace QCConverterTest
{
// ----------------------------------------------------------------------

void oldCodeLength()
{
  SmartMet::Spine::QCConverter tab;

  // Test allowed lengths of old code.
  std::string oldCode = "";
  std::string newCode = "";
  for (int i = 0; i < 6; ++i)
  {
    if (i < 1 and tab.convert(newCode, oldCode))
    {
      TEST_FAILED(
          "QCConverter should return false if the input is less than 1 characters of length.");
    }

    if ((i >= 1 and i <= 4) and !tab.convert(newCode, oldCode))
    {
      TEST_FAILED(
          "QCConverter should return true if the input is between 1 and 4 character of length and "
          "first character is a digit.");
    }

    if (i > 4 and tab.convert(newCode, oldCode))
    {
      TEST_FAILED("QCConverter should return false if the input is over 4 characters of length.");
    }

    oldCode.append("1");
  }

  TEST_PASSED();
}

void conversionResult()
{
  SmartMet::Spine::QCConverter tab;

  std::string oldCode = "";
  std::string newCode = "";

  oldCode = "00";
  if (tab.convert(newCode, oldCode))
  {
    std::cerr << " [" << oldCode << "<- oldcode  newcode-->" << newCode << "]\n";
    TEST_FAILED("QCConverter didn't return false for code '00'.");
  }
  oldCode = "7xxx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("0") != 0))
  {
    std::cerr << " [" << oldCode << "<- oldcode  newcode-->" << newCode << "]\n";
    TEST_FAILED("QCConverter didn't return code '0' for code '7xxx'.");
  }
  oldCode = "1xx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("1") != 0))
    TEST_FAILED("QCConverter didn't return code '1' for code '1xx'.");

  oldCode = "1xxx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("2") != 0))
    TEST_FAILED("QCConverter didn't return code '2' for code '1xxx'.");

  oldCode = "4xx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("3") != 0))
    TEST_FAILED("QCConverter didn't return code '3' for code '4xx'.");

  oldCode = "2xxx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("4") != 0))
    TEST_FAILED("QCConverter didn't return code '4' for code '2xxx'.");

  oldCode = "8x";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("5") != 0))
    TEST_FAILED("QCConverter didn't return code '5' for code '8x'.");

  oldCode = "3x";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("6") != 0))
    TEST_FAILED("QCConverter didn't return code '6' for code '3x'.");

  oldCode = "3xxx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("7") != 0))
    TEST_FAILED("QCConverter didn't return code '7' for code '3xxx'.");

  oldCode = "9xx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("8") != 0))
    TEST_FAILED("QCConverter didn't return code '8' for code '9xx'.");

  oldCode = "9";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("8") != 0))
    TEST_FAILED("QCConverter didn't return code '8' for code '9'.");

  oldCode = "9xxx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("9") != 0))
    TEST_FAILED("QCConverter didn't return code '9' for code '9xxx'.");

  // Not allowed old code
  oldCode = "xx";
  newCode = "";
  if (tab.convert(newCode, oldCode))
    TEST_FAILED("QCConverter did conversion insted of it shouldn't for code 'xx'.");

  TEST_PASSED();
}

void conversionResultOpen()
{
  SmartMet::Spine::QCConverterOpen tab;
  std::string oldCode = "";
  std::string newCode = "";

  oldCode = "0";
  if (!tab.convert(newCode, oldCode))
  {
    std::cerr << " [" << oldCode << "<- oldcode  newcode-->" << newCode << "]\n";
    TEST_FAILED("QCConverterOpen didn't return true for code '0'.");
  }
  oldCode = "7";
  newCode = "";
  if (!tab.convert(newCode, oldCode))
  {
    std::cerr << " [" << oldCode << "<- oldcode  newcode-->" << newCode << "]\n";
    TEST_FAILED("QCConverterOpen didn't return true for code '7'.");
  }
  oldCode = "9";
  newCode = "";
  if (!tab.convert(newCode, oldCode))
  {
    std::cerr << " [" << oldCode << "<- oldcode  newcode-->" << newCode << "]\n";
    TEST_FAILED("QCConverterOpen didn't return false for code '9'.");
  }
  oldCode = "5xx";
  newCode = "";
  if (!tab.convert(newCode, oldCode) or (newCode.compare("3") != 0))
  {
    std::cerr << " [" << oldCode << "<- oldcode  newcode-->" << newCode << "]\n";
    TEST_FAILED("QCConverter didn't return code '3' for code '5xx'.");
  }
  oldCode = "x";
  newCode = "";
  if (tab.convert(newCode, oldCode))
  {
    std::cerr << " [" << oldCode << "<- oldcode  newcode-->" << newCode << "]\n";
    TEST_FAILED("QCConverterOpen didn't return false for character 'x'.");
  }

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
    TEST(oldCodeLength);
    TEST(conversionResult);
    TEST(conversionResultOpen);
  }
};

}  // namespace QCConverterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "QCConverter tester" << endl << "============" << endl;
  QCConverterTest::tests t;
  return t.run();
}

// ======================================================================
