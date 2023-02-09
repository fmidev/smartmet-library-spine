#include "HostInfo.h"
#include <regression/tframe.h>

//! Protection against conflicts with global functions
namespace HostInfoTest
{
// ----------------------------------------------------------------------

void getHostName()
{
  std::string ip = "193.166.221.20";
  std::string expected = "www.fmi.fi";

  auto ret = SmartMet::Spine::HostInfo::getHostName(ip);
  if (ret != expected)
    TEST_FAILED("Failed to resolve " + ip + " to " + expected + ", got '" + ret + "' instead");

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
  void test(void) { TEST(getHostName); }
};

}  // namespace HostInfoTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "HostInfo tester" << endl << "===============" << endl;
  HostInfoTest::tests t;
  return t.run();
}

// ======================================================================
