#include <boost/lexical_cast.hpp>
#include <regression/tframe.h>
#include <Location.h>
#include <iostream>
#include <string>

using namespace std;

namespace LocationFormatterTest
{
void test_longitude()
{
  SmartMet::Spine::Location loc(
      10102, "Helsinki", "fi", 20, "Area", "Feature", "FI", 23., 60., "EEST", 500000, 50, -1);
  string key("longitude");

  if (SmartMet::Spine::formatLocation(loc, key) != "23")
    TEST_FAILED("Longitude conversion failed.");

  TEST_PASSED();
}

void test_population()
{
  SmartMet::Spine::Location loc(
      10102, "Helsinki", "fi", 20, "Area", "Feature", "FI", 23., 6., "EEST", 500000, 5.0, -1);

  string key("population");

  if (SmartMet::Spine::formatLocation(loc, key) != "500000")
    TEST_FAILED("Formatting population 500000 failed");

  TEST_PASSED();
}

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(test_longitude);
    TEST(test_population);
  }
};

}  // namespace LocationFormatterTest

int main()
{
  cout << endl << "LocationFormatter tester" << endl << "========================" << endl;
  LocationFormatterTest::tests t;
  return t.run();
}
