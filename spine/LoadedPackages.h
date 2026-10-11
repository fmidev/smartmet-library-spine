// ======================================================================
/*!
 * \brief Versions of the packages loaded into the running process
 *
 * Every object mapped into the process (the server, engines, plugins
 * and all libraries) is enumerated with dl_iterate_phdr(). The version
 * information is read from the ELF notes of the mapped objects, not
 * from the package database, since after an upgrade without a restart
 * or after 'make install' the database no longer describes what is
 * running:
 *
 *  - .note.package (freedesktop.org packaging metadata, owner "FDO"),
 *    added by rpmbuild on RHEL9 and later
 *  - the "SmartMet" note added by the SmartMet build system
 *    (smartmkpackagenote) with the package name, version and git commit
 *  - the GNU build-id
 *
 * /proc/self/maps tells whether the mapped file has been deleted or
 * replaced on disk. The package manager (rpm, or dpkg-query on Debian
 * based systems) is queried for the currently installed versions, and
 * for the owners of objects which have no notes.
 *
 * Used by the admin request what=packages.
 */
// ======================================================================

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class Table;

namespace HTTP
{
class Request;
}  // namespace HTTP

namespace LoadedPackages
{
// Information embedded in one ELF note of a loaded object
struct Note
{
  std::string type;  // "rpm", "deb", "source" etc
  std::string name;
  std::string version;
  std::string commit;  // SmartMet note only
};

// One object mapped into the process
struct Object
{
  std::string file;                // mapped file
  bool deleted = false;            // mapped file deleted or replaced on disk
  std::string buildid;             // GNU build-id in hex
  std::unique_ptr<Note> fdo;       // .note.package
  std::unique_ptr<Note> smartmet;  // SmartMet note

  // Owning package currently installed on disk, from the package manager
  std::string installed_name;
  std::string installed_version;
  std::string installed_source;  // source package as name-version
  std::string package_manager;   // "rpm", "dpkg" or empty
};

// Parse the notes of a PT_NOTE segment. Alignment is 4 or 8 bytes as
// given by the program header.
void parseNotes(const unsigned char* theData,
                std::size_t theSize,
                std::size_t theAlignment,
                Object& theObject);

// Parse the JSON payload of a FDO or SmartMet note. Returns nullptr if
// the payload is not a JSON object.
std::unique_ptr<Note> parseNoteJson(const std::string& theJson);

// All objects loaded into this process, without package manager information
std::vector<Object> collect();

// Fill in the installed versions from the package manager, if one is available
void resolveInstalled(std::vector<Object>& theObjects);

// Derived values shown in the table. The package name is that of the
// owning package if known, since the notes name the source package.
std::string packageName(const Object& theObject);
std::string runningVersion(const Object& theObject);
bool noteMatchesInstalled(const Object& theObject);
std::string origin(const Object& theObject);
std::string status(const Object& theObject);

// The admin request what=packages. Parameter all=1 lists all objects
// instead of SmartMet packages only.
std::unique_ptr<Table> requestPackages(const HTTP::Request& theRequest);

}  // namespace LoadedPackages
}  // namespace Spine
}  // namespace SmartMet
