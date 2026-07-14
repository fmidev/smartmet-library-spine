// ======================================================================
/*!
 * \file
 * \brief Regression tests for class FileCache
 */
// ======================================================================

#include "FileCache.h"
#include <regression/tframe.h>
#include <sys/types.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

using SmartMet::Spine::FileCache;

namespace
{
std::filesystem::path testdir()
{
  return std::filesystem::path("/tmp") / std::to_string(int(getuid())) / "bsfilecachetest";
}

// Write content to a file, creating the directory if necessary
std::filesystem::path write_file(const std::string& name, const std::string& content)
{
  std::filesystem::create_directories(testdir());
  auto path = testdir() / name;
  std::ofstream out(path.c_str(), std::ios::trunc);
  out << content;
  out.close();
  return path;
}

// Force the modification time forward so a change is detectable regardless of
// filesystem timestamp granularity
void bump_mtime(const std::filesystem::path& path, int seconds)
{
  auto t = std::filesystem::last_write_time(path);
  std::filesystem::last_write_time(path, t + std::chrono::seconds(seconds));
}
}  // namespace

//! Protection against conflicts with global functions
namespace FileCacheTest
{
// ----------------------------------------------------------------------

void basic()
{
  auto path = write_file("basic.txt", "hello");

  FileCache cache;

  if (cache.get(path) != "hello")
    TEST_FAILED("Failed to read back the file contents");

  if (cache.last_modified(path) == 0)
    TEST_FAILED("Modification time should not be zero for an existing file");

  TEST_PASSED();
}

// Within the check window the file is not re-stat'd, so a modified file
// keeps returning the previously cached contents and modification time.
void caches_within_window()
{
  auto path = write_file("window.txt", "A");

  FileCache cache(std::chrono::seconds(3600));

  if (cache.get(path) != "A")
    TEST_FAILED("Failed to read the original contents");

  auto mtime = cache.last_modified(path);

  // Modify the file with a clearly different modification time
  write_file("window.txt", "B");
  bump_mtime(path, 100);

  if (cache.get(path) != "A")
    TEST_FAILED("Contents should still be cached within the check window");

  if (cache.last_modified(path) != mtime)
    TEST_FAILED("Modification time should still be cached within the check window");

  TEST_PASSED();
}

// Once the check window has passed, the change on disk is picked up.
void refresh_after_expiry()
{
  auto path = write_file("expiry.txt", "A");

  FileCache cache(std::chrono::seconds(1));

  if (cache.get(path) != "A")
    TEST_FAILED("Failed to read the original contents");

  auto mtime = cache.last_modified(path);

  write_file("expiry.txt", "B");
  bump_mtime(path, 100);

  // Wait for the check window to expire
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  if (cache.get(path) != "B")
    TEST_FAILED("New contents should be seen after the check window expires");

  if (cache.last_modified(path) == mtime)
    TEST_FAILED("New modification time should be seen after the check window expires");

  TEST_PASSED();
}

// last_modified() is reached only after get(); if the file has been removed
// from disk it must throw, since the product is already incorrect.
void missing_file_throws()
{
  auto path = testdir() / "does_not_exist.txt";
  std::filesystem::remove(path);

  FileCache cache;

  try
  {
    cache.get(path);
    TEST_FAILED("get() should throw for a missing file");
  }
  catch (...)
  {
  }

  try
  {
    cache.last_modified(path);
    TEST_FAILED("last_modified() should throw for a missing file");
  }
  catch (...)
  {
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(basic);
    TEST(caches_within_window);
    TEST(refresh_after_expiry);
    TEST(missing_file_throws);
  }
};

}  // namespace FileCacheTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "FileCache tester" << endl << "================" << endl;
  FileCacheTest::tests t;
  return t.run();
}

// ======================================================================
