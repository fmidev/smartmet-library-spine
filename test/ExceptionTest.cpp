#include "Exception.h"
#include <regression/tframe.h>
#include <future>
#include <iostream>
#include <string>

using std::string;

// Protection against conflicts with global functions
namespace ExceptionTest
{
// ----------------------------------------------------------------------

void test_funct_1()
{
  throw SmartMet::Spine::Exception::Trace(BCP, "Some error");
}

void test_funct_2()
{
  try {
    test_funct_1();
  } catch (...) {
    throw SmartMet::Spine::Exception::Trace(BCP, "Failed");
  }
}

// Test compatibility of SmartMet::Spine::Exception with std::future
void throw_spine_exception_in_async_call()
{
  std::future<void> f = std::async(std::launch::async, &test_funct_1);
  try {
    f.get();
    TEST_FAILED("Exception must be thrown");
  } catch (const std::exception& e) {
    std::cerr << "f.get() -> exception: "<< e.what() << std::endl;
    TEST_PASSED();
  }
}

void throw_nested_spine_exception_in_async_call()
{
  std::future<void> f = std::async(std::launch::async, &test_funct_2);
  try {
    f.get();
    TEST_FAILED("Exception must be thrown");
  } catch (const std::exception& e) {
    std::cerr << "f.get() -> exception: "<< e.what() << std::endl;
    TEST_PASSED();
  }
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
    TEST(throw_spine_exception_in_async_call);
    TEST(throw_nested_spine_exception_in_async_call);
  }
};

}  // namespace ExceptionTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "Exception tester" << endl << "=============" << endl;
  ExceptionTest::tests t;
  return t.run();
}
