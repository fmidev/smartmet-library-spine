#include "LoadedPackages.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Table.h"
#include <fmt/format.h>
#include <json/json.h>
#include <macgyver/Exception.h>
#include <sys/wait.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <link.h>
#include <map>
#include <optional>
#include <set>
#include <spawn.h>
#include <unistd.h>

namespace SmartMet
{
namespace Spine
{
namespace LoadedPackages
{
namespace
{
// freedesktop.org packaging metadata note, see
// https://systemd.io/ELF_PACKAGE_METADATA/
constexpr std::uint32_t fdo_packaging_metadata = 0xcafe1a7e;

// Note written by smartmkpackagenote in smartmet-utils
constexpr std::uint32_t smartmet_package_note = 0x534d0001;

std::size_t align_up(std::size_t theValue, std::size_t theAlignment)
{
  return (theValue + theAlignment - 1) & ~(theAlignment - 1);
}

std::string to_hex(const unsigned char* theData, std::size_t theSize)
{
  std::string ret;
  ret.reserve(2 * theSize);
  for (std::size_t i = 0; i < theSize; i++)
    ret += fmt::format("{:02x}", theData[i]);
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief A file mapping from /proc/self/maps
 */
// ----------------------------------------------------------------------

struct Mapping
{
  std::uintptr_t start = 0;
  std::uintptr_t end = 0;
  std::string file;
  bool deleted = false;
};

std::vector<Mapping> read_maps()
{
  std::vector<Mapping> ret;
  std::ifstream in("/proc/self/maps");
  std::string line;
  const std::string deleted_suffix = " (deleted)";

  // start-end perms offset dev inode path
  while (std::getline(in, line))
  {
    Mapping m;
    int pos = 0;
    if (std::sscanf(
            line.c_str(), "%" SCNxPTR "-%" SCNxPTR " %*s %*s %*s %*s %n", &m.start, &m.end, &pos) <
            2 ||
        pos <= 0)
      continue;

    m.file = line.substr(pos);
    if (m.file.empty() || m.file[0] != '/')
      continue;  // anonymous, [heap], [vdso] etc

    if (m.file.size() > deleted_suffix.size() &&
        m.file.compare(
            m.file.size() - deleted_suffix.size(), deleted_suffix.size(), deleted_suffix) == 0)
    {
      m.file.resize(m.file.size() - deleted_suffix.size());
      m.deleted = true;
    }
    ret.push_back(std::move(m));
  }
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Collected objects during dl_iterate_phdr
 */
// ----------------------------------------------------------------------

struct LoadedObject
{
  std::uintptr_t address = 0;  // address of the first loadable segment
  Object object;
};

int phdr_callback(struct dl_phdr_info* theInfo, std::size_t /* theSize */, void* theData)
{
  // Exceptions must not propagate through the C library
  try
  {
    auto* objects = static_cast<std::vector<LoadedObject>*>(theData);
    LoadedObject obj;

    for (int i = 0; i < theInfo->dlpi_phnum; i++)
    {
      const auto& phdr = theInfo->dlpi_phdr[i];
      if (phdr.p_type == PT_LOAD && obj.address == 0)
        obj.address = theInfo->dlpi_addr + phdr.p_vaddr;
      else if (phdr.p_type == PT_NOTE)
        parseNotes(reinterpret_cast<const unsigned char*>(theInfo->dlpi_addr + phdr.p_vaddr),
                   phdr.p_memsz,
                   phdr.p_align >= 8 ? 8 : 4,
                   obj.object);
    }

    if (obj.address != 0)
      objects->push_back(std::move(obj));
  }
  catch (...)
  {
  }
  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Run a command and return its standard output
 *
 * Returns nothing if the command could not be started. posix_spawn is
 * used instead of fork so that the page tables of a large server process
 * are not copied.
 */
// ----------------------------------------------------------------------

std::optional<std::string> run(const std::vector<std::string>& theArgs)
{
  std::array<int, 2> fds{};
  if (pipe2(fds.data(), O_CLOEXEC) != 0)
    return {};

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, fds[1], 1);
  posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);

  // The server may block or ignore signals, do not pass that on
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  sigset_t mask;
  sigemptyset(&mask);
  posix_spawnattr_setsigmask(&attr, &mask);
  sigset_t defaults;
  sigemptyset(&defaults);
  sigaddset(&defaults, SIGPIPE);
  posix_spawnattr_setsigdefault(&attr, &defaults);
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);

  std::vector<char*> argv;
  argv.reserve(theArgs.size() + 1);
  for (const auto& arg : theArgs)
    argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);

  pid_t pid = 0;
  const int rc = posix_spawnp(&pid, argv[0], &actions, &attr, argv.data(), environ);

  posix_spawnattr_destroy(&attr);
  posix_spawn_file_actions_destroy(&actions);
  close(fds[1]);

  if (rc != 0)
  {
    close(fds[0]);
    return {};
  }

  std::string output;
  std::array<char, 65536> buffer{};
  while (true)
  {
    const ssize_t n = read(fds[0], buffer.data(), buffer.size());
    if (n > 0)
      output.append(buffer.data(), n);
    else if (n == 0 || errno != EINTR)
      break;
  }
  close(fds[0]);

  int wstatus = 0;
  while (waitpid(pid, &wstatus, 0) < 0 && errno == EINTR)
  {
  }

  return output;
}

std::vector<std::string> split(const std::string& theLine, char theSeparator)
{
  std::vector<std::string> ret;
  std::size_t pos = 0;
  while (true)
  {
    const auto next = theLine.find(theSeparator, pos);
    ret.push_back(theLine.substr(pos, next - pos));
    if (next == std::string::npos)
      return ret;
    pos = next + 1;
  }
}

// The package owning a file
struct Owner
{
  std::string name;
  std::string version;
  std::string source;  // source package as name-version
};

// Package owners of files
using Owners = std::map<std::string, Owner>;

// ----------------------------------------------------------------------
/*!
 * \brief Names under which the package databases may know the files
 *
 * With merged /usr the files of many packages are still registered as
 * /lib64/... or /lib/... instead of /usr/lib64/...
 */
// ----------------------------------------------------------------------

std::map<std::string, std::string> file_aliases(const std::set<std::string>& theFiles)
{
  std::map<std::string, std::string> aliases;  // alias --> file
  for (const auto& file : theFiles)
  {
    aliases[file] = file;
    if (file.compare(0, 5, "/usr/") == 0)
      aliases.insert({file.substr(4), file});
  }
  return aliases;
}

// ----------------------------------------------------------------------
/*!
 * \brief Query the owners from the RPM database
 *
 * 'rpm -qf' prints nothing for a file which does not exist, and
 * several lines for a file owned by several packages, so the output
 * cannot be matched to the arguments by position. Instead all files of
 * the owning packages are printed with their package names.
 */
// ----------------------------------------------------------------------

std::optional<Owners> query_rpm(const std::set<std::string>& theFiles)
{
  std::vector<std::string> args{
      "rpm", "-qf", "--qf", "[%{FILENAMES}\t%{=NAME}\t%{=VERSION}-%{=RELEASE}\t%{=SOURCERPM}\n]"};
  args.insert(args.end(), theFiles.begin(), theFiles.end());

  auto output = run(args);
  if (!output)
    return {};

  const auto aliases = file_aliases(theFiles);
  const std::string src_suffix = ".src.rpm";

  Owners ret;
  std::size_t pos = 0;
  while (pos < output->size())
  {
    auto next = output->find('\n', pos);
    if (next == std::string::npos)
      next = output->size();
    const auto fields = split(output->substr(pos, next - pos), '\t');
    pos = next + 1;

    if (fields.size() != 4)
      continue;
    const auto alias = aliases.find(fields[0]);
    if (alias == aliases.end() || ret.count(alias->second) > 0)
      continue;

    Owner owner{fields[1], fields[2], fields[3]};
    if (owner.source.size() > src_suffix.size() &&
        owner.source.compare(
            owner.source.size() - src_suffix.size(), src_suffix.size(), src_suffix) == 0)
      owner.source.resize(owner.source.size() - src_suffix.size());
    else
      owner.source.clear();
    ret[alias->second] = owner;
  }
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Query the owners from the dpkg database
 */
// ----------------------------------------------------------------------

std::optional<Owners> query_dpkg(const std::set<std::string>& theFiles)
{
  const auto aliases = file_aliases(theFiles);

  std::vector<std::string> args{"dpkg-query", "-S"};
  for (const auto& alias : aliases)
    args.push_back(alias.first);

  auto output = run(args);
  if (!output)
    return {};

  // Lines are of the form "pkg1, pkg2: /path"
  std::map<std::string, std::string> packages;  // file --> package
  for (const auto& line : split(*output, '\n'))
  {
    const auto sep = line.find(": /");
    if (sep == std::string::npos || line.compare(0, 10, "diversion ") == 0)
      continue;
    const auto alias = aliases.find(line.substr(sep + 2));
    if (alias == aliases.end() || packages.count(alias->second) > 0)
      continue;
    std::string package = line.substr(0, line.find(','));
    package = package.substr(0, std::min(package.size(), sep));
    packages[alias->second] = package;
  }

  if (packages.empty())
    return Owners{};

  std::set<std::string> names;
  for (const auto& item : packages)
    names.insert(item.second);

  args = {"dpkg-query",
          "-W",
          "-f=${binary:Package}\t${Version}\t${source:Package}-${source:Version}\n"};
  args.insert(args.end(), names.begin(), names.end());

  std::map<std::string, Owner> versions;
  if (auto list = run(args))
  {
    for (const auto& line : split(*list, '\n'))
    {
      const auto fields = split(line, '\t');
      if (fields.size() != 3)
        continue;
      const Owner owner{fields[0].substr(0, fields[0].find(':')), fields[1], fields[2]};
      versions[fields[0]] = owner;
      // -S and -W may disagree on whether the architecture is shown
      versions.insert({owner.name, owner});
    }
  }

  Owners ret;
  for (const auto& item : packages)
  {
    const std::string& package = item.second;
    auto version = versions.find(package);
    if (version == versions.end())
      version = versions.find(package.substr(0, package.find(':')));
    if (version != versions.end())
      ret[item.first] = version->second;
    else
      ret[item.first] = Owner{package.substr(0, package.find(':')), "", ""};
  }
  return ret;
}

std::string text(const Json::Value& theValue, const char* theName)
{
  const auto& value = theValue[theName];
  return value.isString() ? value.asString() : std::string();
}

bool is_smartmet(const Object& theObject)
{
  return packageName(theObject).find("smartmet") != std::string::npos ||
         theObject.file.find("smartmet") != std::string::npos;
}

bool ends_with(const std::string& theString, const std::string& theSuffix)
{
  return theString.size() >= theSuffix.size() &&
         theString.compare(theString.size() - theSuffix.size(), theSuffix.size(), theSuffix) == 0;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Parse the notes of a PT_NOTE segment
 */
// ----------------------------------------------------------------------

void parseNotes(const unsigned char* theData,
                std::size_t theSize,
                std::size_t theAlignment,
                Object& theObject)
{
  std::size_t pos = 0;
  while (pos + sizeof(ElfW(Nhdr)) <= theSize)
  {
    ElfW(Nhdr) header;
    std::memcpy(&header, theData + pos, sizeof(header));

    const std::size_t name_pos = pos + sizeof(header);
    const std::size_t desc_pos = align_up(name_pos + header.n_namesz, theAlignment);
    if (name_pos + header.n_namesz > theSize || desc_pos + header.n_descsz > theSize)
      return;

    const auto* name_ptr = reinterpret_cast<const char*>(theData + name_pos);
    const std::string name(name_ptr, strnlen(name_ptr, header.n_namesz));

    const auto* desc_ptr = theData + desc_pos;
    const auto desc_text = [&]()
    {
      const auto* p = reinterpret_cast<const char*>(desc_ptr);
      return std::string(p, strnlen(p, header.n_descsz));
    };

    if (name == "GNU" && header.n_type == NT_GNU_BUILD_ID)
    {
      if (theObject.buildid.empty())
        theObject.buildid = to_hex(desc_ptr, header.n_descsz);
    }
    else if (name == "FDO" && header.n_type == fdo_packaging_metadata)
    {
      if (!theObject.fdo)
        theObject.fdo = parseNoteJson(desc_text());
    }
    else if (name == "SmartMet" && header.n_type == smartmet_package_note)
    {
      if (!theObject.smartmet)
        theObject.smartmet = parseNoteJson(desc_text());
    }

    pos = align_up(desc_pos + header.n_descsz, theAlignment);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the JSON payload of a package note
 */
// ----------------------------------------------------------------------

std::unique_ptr<Note> parseNoteJson(const std::string& theJson)
{
  Json::Value json;
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string errors;
  if (!reader->parse(theJson.data(), theJson.data() + theJson.size(), &json, &errors) ||
      !json.isObject())
    return nullptr;

  auto note = std::make_unique<Note>();
  note->type = text(json, "type");
  note->name = text(json, "name");
  note->version = text(json, "version");
  note->commit = text(json, "commit");
  return note;
}

// ----------------------------------------------------------------------
/*!
 * \brief All objects loaded into the process
 */
// ----------------------------------------------------------------------

std::vector<Object> collect()
{
  try
  {
    std::vector<LoadedObject> loaded;
    dl_iterate_phdr(phdr_callback, &loaded);

    const auto maps = read_maps();

    std::vector<Object> ret;
    std::set<std::string> seen;
    for (auto& obj : loaded)
    {
      const auto m =
          std::find_if(maps.begin(),
                       maps.end(),
                       [&obj](const Mapping& mapping)
                       { return obj.address >= mapping.start && obj.address < mapping.end; });
      if (m == maps.end())
        continue;  // vdso
      if (!seen.insert(m->file).second)
        continue;
      obj.object.file = m->file;
      obj.object.deleted = m->deleted;
      ret.push_back(std::move(obj.object));
    }
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fill in the installed packages from the package manager
 */
// ----------------------------------------------------------------------

void resolveInstalled(std::vector<Object>& theObjects)
{
  try
  {
    std::set<std::string> files;
    for (const auto& obj : theObjects)
      files.insert(obj.file);

    if (files.empty())
      return;

    std::string manager = "rpm";
    auto owners = query_rpm(files);

    // rpm may also be installed on Debian based systems, but with an empty database
    if (!owners || owners->empty())
    {
      if (auto dpkg_owners = query_dpkg(files))
      {
        if (!dpkg_owners->empty() || !owners)
        {
          owners = std::move(dpkg_owners);
          manager = "dpkg";
        }
      }
    }

    if (!owners)
      return;

    for (auto& obj : theObjects)
    {
      obj.package_manager = manager;
      const auto owner = owners->find(obj.file);
      if (owner != owners->end())
      {
        obj.installed_name = owner->second.name;
        obj.installed_version = owner->second.version;
        obj.installed_source = owner->second.source;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Derived values for the table
 */
// ----------------------------------------------------------------------

std::string packageName(const Object& theObject)
{
  // The notes name the source package, the file may be in a subpackage
  if (!theObject.installed_name.empty())
    return theObject.installed_name;
  if (theObject.fdo && !theObject.fdo->name.empty())
    return theObject.fdo->name;
  if (theObject.smartmet && !theObject.smartmet->name.empty())
    return theObject.smartmet->name;
  return theObject.installed_name;
}

std::string runningVersion(const Object& theObject)
{
  // The notes give the version of the source package, which may differ
  // from that of the subpackage (e.g. nspr built from nss)
  if (noteMatchesInstalled(theObject))
    return theObject.installed_version;
  if (theObject.fdo && !theObject.fdo->version.empty())
    return theObject.fdo->version;
  if (theObject.smartmet)
    return theObject.smartmet->version;
  return {};
}

bool noteMatchesInstalled(const Object& theObject)
{
  const Note* note = (theObject.fdo ? theObject.fdo.get() : theObject.smartmet.get());
  if (!note || theObject.installed_name.empty() || theObject.deleted)
    return false;
  if (!theObject.installed_source.empty())
    return theObject.installed_source == note->name + "-" + note->version;
  return theObject.installed_version == note->version;
}

std::string origin(const Object& theObject)
{
  if (theObject.fdo)
    return (theObject.fdo->type.empty() ? std::string("package") : theObject.fdo->type) + " note";
  if (theObject.smartmet)
    return "smartmet note";
  if (!theObject.installed_name.empty())
    return theObject.package_manager;
  return "unknown";
}

std::string status(const Object& theObject)
{
  if (theObject.deleted)
    return "restart needed";

  if (theObject.smartmet && ends_with(theObject.smartmet->commit, "-dirty"))
    return "dirty";

  const bool has_note = (theObject.fdo || theObject.smartmet);

  // Built outside rpmbuild/dpkg-buildpackage, typically 'make install'
  if (!theObject.fdo && theObject.smartmet && theObject.smartmet->type == "source")
    return "local build";

  if (!has_note)
    return theObject.installed_name.empty() ? "unpackaged" : "ok";

  if (theObject.installed_name.empty())
    return "unpackaged";

  if (!noteMatchesInstalled(theObject))
    return "mismatch";

  return "ok";
}

// ----------------------------------------------------------------------
/*!
 * \brief The admin request what=packages
 */
// ----------------------------------------------------------------------

std::unique_ptr<Table> requestPackages(const HTTP::Request& theRequest)
{
  try
  {
    const bool all = optional_bool(theRequest.getParameter("all"), false);

    auto objects = collect();
    resolveInstalled(objects);

    if (!all)
      objects.erase(
          std::remove_if(
              objects.begin(), objects.end(), [](const Object& obj) { return !is_smartmet(obj); }),
          objects.end());

    std::sort(objects.begin(),
              objects.end(),
              [](const Object& a, const Object& b)
              {
                const auto name_a = packageName(a);
                const auto name_b = packageName(b);
                // Unidentified objects last
                if (name_a.empty() != name_b.empty())
                  return name_b.empty();
                if (name_a != name_b)
                  return name_a < name_b;
                return a.file < b.file;
              });

    auto table = std::make_unique<Table>();
    table->setTitle("Loaded packages");
    table->setNames(
        {"Package", "Version", "Installed", "Commit", "Status", "Origin", "Build-id", "File"});

    std::size_t row = 0;
    for (const auto& obj : objects)
    {
      std::size_t column = 0;
      table->set(column++, row, packageName(obj));
      table->set(column++, row, runningVersion(obj));
      table->set(column++, row, obj.installed_version);
      table->set(column++, row, obj.smartmet ? obj.smartmet->commit : std::string());
      table->set(column++, row, status(obj));
      table->set(column++, row, origin(obj));
      table->set(column++, row, obj.buildid);
      table->set(column++, row, obj.file);
      ++row;
    }

    return table;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace LoadedPackages
}  // namespace Spine
}  // namespace SmartMet
