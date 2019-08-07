#include "Exception.h"
#include <regression/tframe.h>
#include <future>
#include <iostream>
#include <string>

using std::string;

#define TEST_FAILED_UNLESS(pred) if (!(pred)) { TEST_FAILED("Check '" #pred "' failed"); }

// Protection against conflicts with global functions
namespace ExceptionTest
{
// ----------------------------------------------------------------------

void test_std_library_exception_reporting_1()
{
  try {
    try {
      throw std::runtime_error("Foo");
    } catch (...) {
      throw SmartMet::Spine::Exception::Trace(BCP, "Bar");
    }
    TEST_FAILED("Exception should have been thrown");
  } catch (const SmartMet::Spine::Exception& e) {
    TEST_FAILED_UNLESS(e.getWhat() == std::string("Bar"));
    const SmartMet::Spine::Exception* prev = e.getPrevException();
    TEST_FAILED_UNLESS(prev != NULL);
    TEST_FAILED_UNLESS(prev->getWhat() == std::string("[Runtime error] Foo"));
  } catch (...) {
    TEST_FAILED("SmartMet::Spine::Exception was expected");
  }
  TEST_PASSED();
}

struct MyRuntimeError : public std::runtime_error
{
  MyRuntimeError() : std::runtime_error("My runtime error") {}
};

void test_std_library_exception_reporting_2()
{
  try {
    try {
      throw MyRuntimeError();
    } catch (...) {
      throw SmartMet::Spine::Exception::Trace(BCP, "Bar");
    }
    TEST_FAILED("Exception should have been thrown");
  } catch (const SmartMet::Spine::Exception& e) {
    TEST_FAILED_UNLESS(e.getWhat() == std::string("Bar"));
    const SmartMet::Spine::Exception* prev = e.getPrevException();
    TEST_FAILED_UNLESS(prev != NULL);
    TEST_FAILED_UNLESS(prev->getWhat() == std::string("[ExceptionTest::MyRuntimeError] My runtime error"));
  } catch (...) {
    TEST_FAILED("SmartMet::Spine::Exception was expected");
  }
  TEST_PASSED();
}

struct MyException1
{
};

void test_other_exception_reporting_1()
{
  try {
    try {
      throw MyException1();
    } catch (...) {
      throw SmartMet::Spine::Exception::Trace(BCP, "Bar");
    }
    TEST_FAILED("Exception should have been thrown");
  } catch (const SmartMet::Spine::Exception& e) {
    TEST_FAILED_UNLESS(e.getWhat() == std::string("Bar"));
    const SmartMet::Spine::Exception* prev = e.getPrevException();
    TEST_FAILED_UNLESS(prev != NULL);
    TEST_FAILED_UNLESS(prev->getWhat() == std::string("[ExceptionTest::MyException1]"));
  } catch (...) {
    TEST_FAILED("SmartMet::Spine::Exception was expected");
  }
  TEST_PASSED();
}

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
  } catch (const SmartMet::Spine::Exception& e) {
    if (e.getWhat() != std::string("Some error")) {
      TEST_FAILED("Unexpected value of exception message");
    }
    const SmartMet::Spine::Exception* prev = e.getPrevException();
    if (prev) {
      TEST_FAILED("There should be no previous exception");
    }
    //std::cout << "f.get() -> exception:\n";
    //e.printError();
    TEST_PASSED();
  } catch (...) {
    TEST_FAILED("Unexpected exception type");
  }
}

void throw_nested_spine_exception_in_async_call()
{
  std::future<void> f = std::async(std::launch::async, &test_funct_2);
  try {
    f.get();
    TEST_FAILED("Exception must be thrown");
  } catch (const SmartMet::Spine::Exception& e) {
    //std::cout << "f.get() -> exception:\n";
    //e.printError();
    if (e.getWhat() != std::string("Failed")) {
      TEST_FAILED("Unexpected value of exception message");
    }
    const SmartMet::Spine::Exception* prev = e.getPrevException();
    if (prev) {
      if (prev->getWhat() != std::string("Some error")) {
	TEST_FAILED("Unexpected value of exception message");
      }
      prev = prev->getPrevException();
      if (prev) {
	TEST_FAILED("There should be only 2 exception levels");
      }
    } else {
      TEST_FAILED("There should be no previous exception");
    }
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
    TEST(test_std_library_exception_reporting_1);
    TEST(test_std_library_exception_reporting_2);
    TEST(test_other_exception_reporting_1);
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
