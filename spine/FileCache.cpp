// ======================================================================

#include "FileCache.h"
#include <macgyver/Exception.h>
#include <fstream>
#include <sys/stat.h>

namespace SmartMet
{
namespace Spine
{
namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Modification time and size of a file with a single stat call
 *
 * Both are zero for a file which cannot be stat'd. We do not use
 * Fmi::last_write_time, since we need the size from the very same call: two
 * separate queries would mean two stat calls per validation.
 */
// ----------------------------------------------------------------------

FileCache::Stamp stat_file(const std::filesystem::path& thePath)
{
  struct stat info
  {
  };

  if (::stat(thePath.c_str(), &info) != 0)
    return {};

  return {static_cast<std::size_t>(info.st_mtime), static_cast<std::size_t>(info.st_size)};
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

FileCache::FileCache(std::chrono::seconds theMaxCheckAge) : itsMaxCheckAge(theMaxCheckAge) {}

// ----------------------------------------------------------------------
/*!
 * \brief Set the maximum age of a modification time check
 */
// ----------------------------------------------------------------------

void FileCache::setMaxCheckAge(std::chrono::seconds theMaxCheckAge)
{
  WriteLock lock(itsMutex);
  itsMaxCheckAge = theMaxCheckAge;
}

// ----------------------------------------------------------------------
/*!
 * \brief (Re)validate the cache entry for the given path
 *
 * Stats the file and either refreshes the check time of an unchanged
 * cached entry, or reads the file contents and (re)inserts them. Reading
 * the file is done without holding a lock. The returned contents are a
 * copy so they remain valid after the lock is released.
 *
 * A cached entry is considered unchanged only if both the modification time
 * and the size still match. The modification time has a resolution of one
 * second, so comparing the size too catches a rewrite within the same second
 * whenever it alters the length of the file. Both come from the same stat call.
 */
// ----------------------------------------------------------------------

FileCache::FileContents FileCache::refresh(const std::filesystem::path& thePath,
                                           std::chrono::steady_clock::time_point now) const
{
  // Modification time and size, tolerating a missing file. A missing/unreadable
  // file is cached as a negative result (modification_time == 0) so that repeated
  // modification-time checks (ETag/hash calculation) are throttled exactly like
  // existing files instead of re-stat'ing on every call. get() turns the
  // negative result into an exception; last_modified() simply returns 0.
  const auto disk = stat_file(thePath);

  if (disk.modification_time == 0)
  {
    FileContents missing(0, now, std::string());
    WriteLock lock(itsMutex);
    itsCache[thePath] = missing;
    return missing;
  }

  // If the file has not changed, just refresh the check time and reuse the contents
  {
    WriteLock lock(itsMutex);
    auto iter = itsCache.find(thePath);
    if (iter != itsCache.end() &&
        disk.modification_time == static_cast<std::size_t>(iter->second.modification_time) &&
        disk.size == iter->second.content.size())
    {
      iter->second.checked_time = now;
      return iter->second;
    }
  }

  // No active lock while we read the file contents

  std::string content;
  std::ifstream in(thePath.c_str());
  if (!in)
    throw Fmi::Exception(BCP, "Failed to open '" + thePath.string() + "' for reading!");

  content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

  // Now insert the value into the cache and return it

  FileContents contents(
      static_cast<std::time_t>(disk.modification_time), now, std::move(content));
  {
    WriteLock lock(itsMutex);
    itsCache[thePath] = contents;
  }
  return contents;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get file contents
 */
// ----------------------------------------------------------------------

std::string FileCache::get(const std::filesystem::path& thePath) const
{
  try
  {
    const auto now = std::chrono::steady_clock::now();

    // Fast path: return the cached contents if the last check is recent enough
    {
      ReadLock lock(itsMutex);
      auto iter = itsCache.find(thePath);
      if (iter != itsCache.end() && now - iter->second.checked_time < itsMaxCheckAge)
      {
        if (iter->second.modification_time == 0)
          throw Fmi::Exception(BCP, "Failed to open '" + thePath.string() + "' for reading!");
        return iter->second.content;
      }
    }

    // The check has expired (or the file is not cached): validate against the disk
    const auto contents = refresh(thePath, now);
    if (contents.modification_time == 0)
      throw Fmi::Exception(BCP, "Failed to open '" + thePath.string() + "' for reading!");
    return contents.content;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the file modification time
 *
 * This is used only for ETag calculation, and is called only after the
 * file has already been requested with get(). Hence the file is normally
 * already cached; if not, we simply load it again.
 */
// ----------------------------------------------------------------------

std::size_t FileCache::last_modified(const std::filesystem::path& thePath) const
{
  try
  {
    const auto now = std::chrono::steady_clock::now();

    // Fast path: return the cached modification time if the last check is recent enough
    {
      ReadLock lock(itsMutex);
      auto iter = itsCache.find(thePath);
      if (iter != itsCache.end() && now - iter->second.checked_time < itsMaxCheckAge)
        return iter->second.modification_time;
    }

    // The check has expired (or the file is not cached): validate against the disk
    return refresh(thePath, now).modification_time;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the modification time and the size of the file
 *
 * As last_modified(), but also returns the size of the contents. The cache
 * holds the contents already, so the size costs nothing: no more stat calls
 * are made than for the modification time alone.
 *
 * The size makes hash values sensitive to changes which preserve the
 * modification time, which has a resolution of one second only.
 */
// ----------------------------------------------------------------------

FileCache::Stamp FileCache::stamp(const std::filesystem::path& thePath) const
{
  try
  {
    const auto now = std::chrono::steady_clock::now();

    // Fast path: return the cached values if the last check is recent enough
    {
      ReadLock lock(itsMutex);
      auto iter = itsCache.find(thePath);
      if (iter != itsCache.end() && now - iter->second.checked_time < itsMaxCheckAge)
        return {static_cast<std::size_t>(iter->second.modification_time),
                iter->second.content.size()};
    }

    // The check has expired (or the file is not cached): validate against the disk
    const auto contents = refresh(thePath, now);
    return {static_cast<std::size_t>(contents.modification_time), contents.content.size()};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
