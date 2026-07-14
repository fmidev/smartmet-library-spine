// ======================================================================
/*!
 * \brief Generic file content cache
 *
 * This cache stores permanently all requested files. The file contents
 * are never expired from the cache: once a file has been read, its
 * contents are kept until they are observed to have changed on disk.
 *
 * To avoid a stat() call on every access, the file modification time is
 * only re-checked once the previous check is older than a configurable
 * maximum age (default 10 seconds). Within that window the cached
 * contents and modification time are returned without touching the disk.
 * The window bounds the staleness: a file modified on disk is picked up
 * after at most the maximum check age.
 *
 * Files may be accessed from multiple threads. We do not return
 * references since a value may be replaced by a new insertion into the
 * cache. We do not return a shared pointer either, since the value is
 * almost always going to be copied into a CDT structure anyway.
 */
// ======================================================================

#pragma once

#include "Thread.h"
#include <chrono>
#include <filesystem>
#include <map>
#include <string>

namespace SmartMet
{
namespace Spine
{
class FileCache
{
 public:
  explicit FileCache(std::chrono::seconds theMaxCheckAge = std::chrono::seconds(10));

  std::string get(const std::filesystem::path& thePath) const;
  std::size_t last_modified(const std::filesystem::path& thePath) const;

  // Set the maximum age of a modification time check before the file is stat'd again
  void setMaxCheckAge(std::chrono::seconds theMaxCheckAge);

 private:
  struct FileContents
  {
    std::time_t modification_time = 0;
    std::chrono::steady_clock::time_point checked_time;
    std::string content;

    FileContents() = default;
    FileContents(std::time_t theTime,
                 std::chrono::steady_clock::time_point theChecked,
                 std::string theContent)
        : modification_time(theTime), checked_time(theChecked), content(std::move(theContent))
    {
    }
  };

  // (Re)validate the cache entry for thePath, reading the file if needed, and return it
  FileContents refresh(const std::filesystem::path& thePath,
                       std::chrono::steady_clock::time_point now) const;

  using Cache = std::map<std::filesystem::path, FileContents>;
  mutable MutexType itsMutex;
  mutable Cache itsCache;
  std::chrono::steady_clock::duration itsMaxCheckAge;

};  // class FileCache

}  // namespace Spine
}  // namespace SmartMet
