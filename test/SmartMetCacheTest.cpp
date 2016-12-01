// ======================================================================
/*!
 * \file
 * \brief Regression tests for class Table
 */
// ======================================================================

#include "SmartMetCache.h"
#include <regression/tframe.h>
#include <sstream>

#include <boost/make_shared.hpp>

template <typename T>
std::string tostr(const T& theValue)
{
  std::ostringstream out;
  out << theValue;
  return out.str();
}

//! Protection against conflicts with global functions
namespace CacheTest
{
// ----------------------------------------------------------------------

void basic()
{
  SmartMet::Spine::SmartMetCache cache(16, 11, "/tmp/bscachetest");

  std::vector<std::size_t> keys = {1, 2, 3, 4, 5};

  std::vector<std::string> values = {"first", "second", "third", "fourth", "fifth"};

  std::vector<std::size_t> correct_order = {3, 4, 5, 1, 2};

  for (int i = 0; i < 5; ++i)
  {
    cache.insert(keys[i], boost::make_shared<std::string>(values[i]));
  }

  boost::this_thread::sleep(boost::posix_time::seconds(2));

  auto content = cache.getContent();

  if (content.size() != 5)
  {
    auto size = tostr(content.size());
    TEST_FAILED("Incorrect content size '" + size + "', should be 5");
  }

  for (unsigned int i = 0; i < content.size(); ++i)
  {
    auto key = content[i].first;
    if (key != correct_order[i])
    {
      auto keystr = tostr(key);
      auto correctstr = tostr(correct_order[i]);
      auto index = tostr(i);
      TEST_FAILED("Incorrect content at index '" + index + "', expected '" + correctstr +
                  "', got '" + keystr + "'");
    }
  }

  TEST_PASSED();
}

void find()
{
  SmartMet::Spine::SmartMetCache cache(3, 2, "/tmp/bscachetest2");

  std::vector<std::size_t> keys = {1, 2, 3, 4, 5, 6, 7};

  std::vector<std::string> values = {"1", "2", "3", "4", "5", "6", "7"};

  std::vector<std::size_t> correct_order = {6, 5, 4, 4, 7};

  for (int i = 0; i < 7; ++i)
  {
    cache.insert(keys[i], boost::make_shared<std::string>(values[i]));
  }

  boost::this_thread::sleep(boost::posix_time::seconds(1));

  std::vector<std::size_t> search = {7, 6, 5, 4};

  for (auto number : search)
  {
    auto res = cache.find(number);
    std::string s = tostr(number);
    if (*res != s)
    {
      TEST_FAILED("Incorrect result on key '" + s + "' should be '" + values[number - 1] +
                  "', got '" + *res + "'");
    }
    boost::this_thread::sleep(boost::posix_time::millisec(500));
  }

  auto content = cache.getContent();

  for (unsigned int i = 0; i < content.size(); ++i)
  {
    auto key = content[i].first;
    if (key != correct_order[i])
    {
      auto keystr = tostr(key);
      auto correctstr = tostr(correct_order[i]);
      auto index = tostr(i);
      TEST_FAILED("Incorrect content at index '" + index + "', expected '" + correctstr +
                  "', got '" + keystr + "'");
    }
  }

  TEST_PASSED();
}

void promote()
{
  SmartMet::Spine::SmartMetCache cache(10, 10, "/tmp/bscachetest3");

  std::vector<std::size_t> keys1 = {1, 2, 3, 4, 5};

  std::vector<std::string> values1 = {"1", "2", "3", "4", "5"};

  std::vector<std::size_t> keys2 = {10, 20, 30, 40, 50};

  std::vector<std::string> values2 = {"10", "20", "30", "40", "50"};

  std::vector<std::size_t> correct_order = {4, 5, 10, 20, 30, 40, 50, 30, 1, 40, 2, 3};

  for (int i = 0; i < 5; ++i)
  {
    cache.insert(keys2[i], boost::make_shared<std::string>(values2[i]));
  }

  for (int i = 0; i < 5; ++i)
  {
    cache.insert(keys1[i], boost::make_shared<std::string>(values1[i]));
  }

  // Wait for disk flush
  boost::this_thread::sleep(boost::posix_time::seconds(1));

  for (int i = 0; i < 4; ++i)
  {
    auto res = cache.find((unsigned)keys2[i]);
    boost::this_thread::sleep(boost::posix_time::millisec(1000));
  }

  auto content = cache.getContent();

  for (unsigned int i = 0; i < content.size(); ++i)
  {
    auto key = content[i].first;
    if (key != correct_order[i])
    {
      auto keystr = tostr(key);
      auto correctstr = tostr(correct_order[i]);
      auto index = tostr(i);
      TEST_FAILED("Incorrect content at index '" + index + "', expected '" + correctstr +
                  "', got '" + keystr + "'");
    }
  }

  TEST_PASSED();

  // auto ignored = cache.find(5);
  // ignored = cache.find(4);
  // ignored = cache.find(3);
  // ignored = cache.find(2);
  // ignored = cache.find(1);

  // boost::this_thread::sleep(boost::posix_time::seconds(1));

  // auto content = cache.getContent();

  // for (unsigned int i = 0; i<content.size(); ++i)
  //   {
  // 	auto key = content[i].first;
  // 	if (key != correct_order[i])
  // 	  {
  // 		auto keystr = tostr(key);
  // 		auto correctstr = tostr(correct_order[i]);
  // 		auto index = tostr(i);
  // 		TEST_FAILED("Incorrect content at index '" + index + "', expected '" + correctstr +
  // "',
  // got '" + keystr + "'");
  // 	  }

  //   }

  // TEST_PASSED();
}

// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(basic);
    TEST(find);
    TEST(promote);
  }
};

}  // namespace CacheTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "Cache tester" << endl << "============" << endl;
  CacheTest::tests t;
  return t.run();
}

// ======================================================================
