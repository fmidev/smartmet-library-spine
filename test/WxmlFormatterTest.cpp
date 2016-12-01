// ======================================================================
/*!
 * \file
 * \brief Regression tests for class WxmlFormatter
 */
// ======================================================================

#include "WxmlFormatter.h"
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
namespace WxmlFormatterTest
{
// ----------------------------------------------------------------------

void version1()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");

  names.push_back("origintime");
  names.push_back("xmltime");
  names.push_back("weekday");
  names.push_back("timestring");
  names.push_back("name");
  names.push_back("geoid");
  names.push_back("longitude");
  names.push_back("latitude");

  tab.set(0, 0, "0");
  tab.set(0, 1, "1");
  tab.set(1, 0, "2");
  tab.set(1, 1, "3");

  for (int i = 0; i < 2; i++)
  {
    tab.set(2, i, "origintime" + tostr(i));
    tab.set(3, i, "xmltime" + tostr(i));
    tab.set(4, i, "weekday" + tostr(i));
    tab.set(5, i, "timestring" + tostr(i));
    tab.set(6, i, "name" + tostr(i));
    tab.set(7, i, "geoid" + tostr(i));
    tab.set(8, i, "longitude" + tostr(i));
    tab.set(9, i, "latitude" + tostr(i));
  }

  const char* res =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<pointweather "
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
      "xsi:schemaLocation=\"http://services.weatherproof.fi/schemas/"
      "pointweather_1.00.xsd\">\n<meta>\n<updated>origintime1</updated>\n</meta>\n<location "
      "name=\"name0\" id=\"geoid0\" lon=\"longitude0\" lat=\"latitude0\">\n<forecast "
      "time=\"xmltime0\" day=\"weekday0\">\n<param name=\"col0\" value=\"0\"/>\n<param "
      "name=\"col1\" value=\"2\"/>\n</forecast>\n</location>\n<location name=\"name1\" "
      "id=\"geoid1\" lon=\"longitude1\" lat=\"latitude1\">\n<forecast time=\"xmltime1\" "
      "day=\"weekday1\">\n<param name=\"col0\" value=\"1\"/>\n<param name=\"col1\" "
      "value=\"3\"/>\n</forecast>\n</location>\n</pointweather>\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("version", "1.00");

  SmartMet::Spine::WxmlFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result: " + out.str() + "\nExpected result: " + res);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void version2()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;
  names.push_back("col0");
  names.push_back("col1");

  names.push_back("origintime");
  names.push_back("xmltime");
  names.push_back("weekday");
  names.push_back("timestring");
  names.push_back("name");
  names.push_back("geoid");
  names.push_back("longitude");
  names.push_back("latitude");

  tab.set(0, 0, "0");
  tab.set(0, 1, "1");
  tab.set(1, 0, "2");
  tab.set(1, 1, "3");

  for (int i = 0; i < 2; i++)
  {
    tab.set(2, i, "origintime" + tostr(i));
    tab.set(3, i, "xmltime" + tostr(i));
    tab.set(4, i, "weekday" + tostr(i));
    tab.set(5, i, "timestring" + tostr(i));
    tab.set(6, i, "name" + tostr(i));
    tab.set(7, i, "geoid" + tostr(i));
    tab.set(8, i, "longitude" + tostr(i));
    tab.set(9, i, "latitude" + tostr(i));
  }

  const char* res =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<pointweather "
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
      "xsi:schemaLocation=\"http://services.weatherproof.fi/schemas/"
      "pointweather_2.00.xsd\">\n<meta>\n<updated>origintime1</updated>\n</meta>\n<location "
      "name=\"name0\" id=\"geoid0\" lon=\"longitude0\" lat=\"latitude0\">\n<forecast "
      "time=\"xmltime0\" timestring=\"timestring0\">\n<param name=\"col0\" value=\"0\"/>\n<param "
      "name=\"col1\" value=\"2\"/>\n</forecast>\n</location>\n<location name=\"name1\" "
      "id=\"geoid1\" lon=\"longitude1\" lat=\"latitude1\">\n<forecast time=\"xmltime1\" "
      "timestring=\"timestring1\">\n<param name=\"col0\" value=\"1\"/>\n<param name=\"col1\" "
      "value=\"3\"/>\n</forecast>\n</location>\n</pointweather>\n";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("version", "2.00");

  SmartMet::Spine::WxmlFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() != res)
    TEST_FAILED("Incorrect result: " + out.str() + "\nExpected result: " + res);

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

  SmartMet::Spine::WxmlFormatter fmt;
  std::ostringstream out;
  fmt.format(out, tab, names, req, config);

  if (out.str() !=
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<pointweather "
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
      "xsi:schemaLocation=\"http://services.weatherproof.fi/schemas/pointweather_2.00.xsd\">\n</"
      "pointweather>")
    TEST_FAILED("Incorrect result: " + out.str());

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
    TEST(version1);
    TEST(version2);
    TEST(empty);
  }
};

}  // namespace WxmlFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "WxmlFormatter tester" << endl << "====================" << endl;
  WxmlFormatterTest::tests t;
  return t.run();
}

// ======================================================================
