// ======================================================================

#include "FileCache.h"
#include <macgyver/Exception.h>
#include <macgyver/FileSystem.h>
#include <fstream>

namespace SmartMet
{
namespace Spine
{
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
 */
// ----------------------------------------------------------------------

FileCache::FileContents FileCache::refresh(const std::filesystem::path& thePath,
                                           std::chrono::steady_clock::time_point now) const
{
  // Modification time, tolerating a missing file. A missing/unreadable file is
  // cached as a negative result (modification_time == 0) so that repeated
  // modification-time checks (ETag/hash calculation) are throttled exactly like
  // existing files instead of re-stat'ing on every call. get() turns the
  // negative result into an exception; last_modified() simply returns 0.
  std::error_code ec;
  std::time_t mtime = Fmi::last_write_time(thePath, ec);
  if (ec)
    mtime = 0;

  if (mtime == 0)
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
    if (iter != itsCache.end() && mtime == iter->second.modification_time)
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

  FileContents contents(mtime, now, std::move(content));
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

}  // namespace Spine
}  // namespace SmartMet
