#include "HTTP.h"
#include "LoadedPackages.h"
#include "Table.h"
#include <regression/tframe.h>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <filesystem>
#include <link.h>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

//! Protection against conflicts with global functions
namespace LoadedPackagesTest
{
namespace LP = SmartMet::Spine::LoadedPackages;

// Append a note to a PT_NOTE style buffer with the given alignment
void add_note(std::vector<unsigned char>& theBuffer,
              const std::string& theOwner,
              std::uint32_t theType,
              const std::string& theDesc,
              std::size_t theAlignment)
{
  const auto pad = [&]()
  {
    while (theBuffer.size() % theAlignment != 0)
      theBuffer.push_back(0);
  };

  const std::uint32_t namesz = theOwner.size() + 1;
  const std::uint32_t descsz = theDesc.size();
  for (std::uint32_t value : {namesz, descsz, theType})
  {
    const auto* p = reinterpret_cast<const unsigned char*>(&value);
    theBuffer.insert(theBuffer.end(), p, p + sizeof(value));
  }
  theBuffer.insert(theBuffer.end(), theOwner.begin(), theOwner.end());
  theBuffer.push_back(0);
  pad();
  theBuffer.insert(theBuffer.end(), theDesc.begin(), theDesc.end());
  pad();
}

void parse_notes()
{
  std::vector<unsigned char> buffer;
  add_note(buffer, "GNU", NT_GNU_BUILD_ID, std::string("\x01\x02\xab\xff", 4), 4);
  add_note(buffer,
           "FDO",
           0xcafe1a7e,
           R"({"type":"rpm","name":"smartmet-library-spine","version":"26.10.11-1.el10.fmi"})" +
               std::string(1, '\0'),
           4);
  add_note(
      buffer,
      "SmartMet",
      0x534d0001,
      R"({"type":"rpm","name":"smartmet-library-spine","version":"26.10.11-1.el10.fmi","commit":"0123456789ab"})",
      4);
  add_note(buffer, "Other", 1, "ignored", 4);

  LP::Object obj;
  LP::parseNotes(buffer.data(), buffer.size(), 4, obj);

  if (obj.buildid != "0102abff")
    TEST_FAILED("Build-id should be 0102abff, got '" + obj.buildid + "'");
  if (!obj.fdo)
    TEST_FAILED("FDO note not found");
  if (obj.fdo->type != "rpm" || obj.fdo->name != "smartmet-library-spine" ||
      obj.fdo->version != "26.10.11-1.el10.fmi")
    TEST_FAILED("FDO note parsed incorrectly");
  if (!obj.smartmet)
    TEST_FAILED("SmartMet note not found");
  if (obj.smartmet->commit != "0123456789ab")
    TEST_FAILED("SmartMet commit should be 0123456789ab, got '" + obj.smartmet->commit + "'");

  TEST_PASSED();
}

void parse_notes_aligned_8()
{
  // .note.gnu.property style segment: padding to 8 bytes, the name ends
  // at offset 16 so the descriptor follows without padding
  std::vector<unsigned char> buffer;
  add_note(buffer, "GNU", 5, std::string(16, '\x01'), 8);
  add_note(buffer, "GNU", NT_GNU_BUILD_ID, std::string("\xde\xad", 2), 8);

  LP::Object obj;
  LP::parseNotes(buffer.data(), buffer.size(), 8, obj);

  if (obj.buildid != "dead")
    TEST_FAILED("Build-id should be dead, got '" + obj.buildid + "'");

  TEST_PASSED();
}

void parse_truncated()
{
  std::vector<unsigned char> buffer;
  add_note(buffer, "GNU", NT_GNU_BUILD_ID, std::string("\x01\x02\x03\x04", 4), 4);
  add_note(buffer, "FDO", 0xcafe1a7e, R"({"type":"rpm","name":"x","version":"1-1"})", 4);

  // Cut in the middle of the second note
  LP::Object obj;
  LP::parseNotes(buffer.data(), buffer.size() - 10, 4, obj);

  if (obj.buildid != "01020304")
    TEST_FAILED("First note should be parsed, got build-id '" + obj.buildid + "'");
  if (obj.fdo)
    TEST_FAILED("Truncated note must be ignored");

  TEST_PASSED();
}

void parse_json()
{
  if (LP::parseNoteJson("not json"))
    TEST_FAILED("Invalid JSON should be rejected");
  if (LP::parseNoteJson("[1,2]"))
    TEST_FAILED("Non-object JSON should be rejected");

  auto note = LP::parseNoteJson(R"({"type":"deb","name":"libc6","version":2})");
  if (!note || note->type != "deb" || note->name != "libc6" || !note->version.empty())
    TEST_FAILED("Non-string fields should be left empty");

  TEST_PASSED();
}

LP::Object make_object(const std::string& theType,
                       const std::string& theVersion,
                       const std::string& theInstalled,
                       const std::string& theCommit = "")
{
  LP::Object obj;
  obj.file = "/usr/lib64/libsmartmet-test.so";
  obj.smartmet = std::make_unique<LP::Note>();
  obj.smartmet->type = theType;
  obj.smartmet->name = "smartmet-library-test";
  obj.smartmet->version = theVersion;
  obj.smartmet->commit = theCommit;
  if (!theInstalled.empty())
  {
    obj.installed_name = "smartmet-library-test";
    obj.installed_version = theInstalled;
    obj.package_manager = "rpm";
  }
  return obj;
}

void status()
{
  auto ok = make_object("rpm", "1.0-1", "1.0-1");
  if (LP::status(ok) != "ok")
    TEST_FAILED("Expected ok, got " + LP::status(ok));

  auto mismatch = make_object("rpm", "1.0-1", "1.0-2");
  if (LP::status(mismatch) != "mismatch")
    TEST_FAILED("Expected mismatch, got " + LP::status(mismatch));

  auto deleted = make_object("rpm", "1.0-1", "1.0-2");
  deleted.deleted = true;
  if (LP::status(deleted) != "restart needed")
    TEST_FAILED("Expected restart needed, got " + LP::status(deleted));

  auto dirty = make_object("source", "1.0-1", "1.0-1", "0123456789ab-dirty");
  if (LP::status(dirty) != "dirty")
    TEST_FAILED("Expected dirty, got " + LP::status(dirty));

  auto local = make_object("source", "1.0-1", "1.0-1", "0123456789ab");
  if (LP::status(local) != "local build")
    TEST_FAILED("Expected local build, got " + LP::status(local));

  auto unowned = make_object("rpm", "1.0-1", "");
  if (LP::status(unowned) != "unpackaged")
    TEST_FAILED("Expected unpackaged, got " + LP::status(unowned));

  // Subpackage with its own version, e.g. nspr built from nss
  LP::Object sub;
  sub.file = "/usr/lib64/libnspr4.so";
  sub.fdo = std::make_unique<LP::Note>();
  sub.fdo->type = "rpm";
  sub.fdo->name = "nss";
  sub.fdo->version = "3.124.0-5.el10_2";
  sub.installed_name = "nspr";
  sub.installed_version = "4.39.0-5.el10_2";
  sub.installed_source = "nss-3.124.0-5.el10_2";
  sub.package_manager = "rpm";
  if (LP::status(sub) != "ok" || LP::runningVersion(sub) != "4.39.0-5.el10_2" ||
      LP::packageName(sub) != "nspr")
    TEST_FAILED("Subpackage should match its source package, got " + LP::status(sub));

  sub.installed_source = "nss-3.125.0-1.el10_2";
  if (LP::status(sub) != "mismatch" || LP::runningVersion(sub) != "3.124.0-5.el10_2")
    TEST_FAILED("Upgraded source package should be a mismatch, got " + LP::status(sub));

  LP::Object nothing;
  nothing.file = "/opt/libfoo.so";
  if (LP::status(nothing) != "unpackaged" || LP::origin(nothing) != "unknown")
    TEST_FAILED("Object without any information should be unpackaged and of unknown origin");

  LP::Object rpmonly;
  rpmonly.file = "/usr/lib64/libfoo.so";
  rpmonly.installed_name = "foo";
  rpmonly.installed_version = "1-1";
  rpmonly.package_manager = "rpm";
  if (LP::status(rpmonly) != "ok" || LP::origin(rpmonly) != "rpm" ||
      LP::packageName(rpmonly) != "foo" || !LP::runningVersion(rpmonly).empty())
    TEST_FAILED("Object known only to rpm reported incorrectly");

  TEST_PASSED();
}

void collect()
{
  const auto objects = LP::collect();

  bool found_spine = false;
  bool found_libc = false;
  for (const auto& obj : objects)
  {
    if (obj.file.empty() || obj.file[0] != '/')
      TEST_FAILED("Object without an absolute path: '" + obj.file + "'");
    if (obj.file.find("libsmartmet-spine.so") != std::string::npos)
    {
      found_spine = true;
      if (obj.buildid.empty())
        TEST_FAILED("libsmartmet-spine.so should have a build-id");
      if (obj.deleted)
        TEST_FAILED("libsmartmet-spine.so should not be marked deleted");
    }
    if (obj.file.find("/libc.so") != std::string::npos)
      found_libc = true;
  }

  if (!found_spine)
    TEST_FAILED("libsmartmet-spine.so not found among the loaded objects");
  if (!found_libc)
    TEST_FAILED("libc not found among the loaded objects");

  TEST_PASSED();
}

// Path of the loaded libz, which every test links indirectly
std::string find_libz()
{
  for (const auto& obj : LP::collect())
    if (obj.file.find("/libz.so") != std::string::npos)
      return obj.file;
  return {};
}

void deleted()
{
  // Load a copy of a library, then remove the copy as a package upgrade would
  const std::string libz = find_libz();
  if (libz.empty())
    TEST_FAILED("libz not loaded, cannot test deleted libraries");

  const auto copy = std::filesystem::temp_directory_path() /
                    ("LoadedPackagesTest-" + std::to_string(getpid()) + "-libz.so");
  std::filesystem::copy_file(libz, copy, std::filesystem::copy_options::overwrite_existing);

  void* handle = dlopen(copy.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle)
  {
    std::filesystem::remove(copy);
    TEST_FAILED(std::string("dlopen failed: ") + dlerror());
  }

  auto find_copy = [&copy]() -> std::optional<LP::Object>
  {
    for (auto& obj : LP::collect())
      if (obj.file == copy.string())
        return std::move(obj);
    return {};
  };

  const auto before = find_copy();
  std::filesystem::remove(copy);
  const auto after = find_copy();
  dlclose(handle);

  if (!before)
    TEST_FAILED("Copied library not found among the loaded objects");
  if (before->deleted)
    TEST_FAILED("Copied library should not be deleted before removal");
  if (!after)
    TEST_FAILED("Removed library not found among the loaded objects");
  if (!after->deleted || LP::status(*after) != "restart needed")
    TEST_FAILED("Removed library should need a restart, got " + LP::status(*after));
  if (after->buildid.empty() || after->buildid != before->buildid)
    TEST_FAILED("Build-id should still be readable from the mapped library");

  TEST_PASSED();
}

void request()
{
  SmartMet::Spine::HTTP::Request req;
  req.addParameter("all", "1");
  auto all = LP::requestPackages(req);

  SmartMet::Spine::HTTP::Request req2;
  auto smartmet = LP::requestPackages(req2);

  if (all->empty())
    TEST_FAILED("all=1 should list the loaded objects");
  if (smartmet->maxj() >= all->maxj() && !all->empty())
    TEST_FAILED("Default listing should be a subset of all=1");

  TEST_PASSED();
}

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(parse_notes);
    TEST(parse_notes_aligned_8);
    TEST(parse_truncated);
    TEST(parse_json);
    TEST(status);
    TEST(collect);
    TEST(deleted);
    TEST(request);
  }
};

}  // namespace LoadedPackagesTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "LoadedPackages tester" << endl << "=====================" << endl;
  LoadedPackagesTest::tests t;
  return t.run();
}

// ======================================================================
