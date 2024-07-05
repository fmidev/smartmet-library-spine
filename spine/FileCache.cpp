// ======================================================================

#include "FileCache.h"
#include <boost/filesystem/operations.hpp>
#include <macgyver/Exception.h>
#include <macgyver/FileSystem.h>
#include <fstream>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Get file contents
 */
// ----------------------------------------------------------------------

std::string FileCache::get(const std::filesystem::path& thePath) const
{
  try
  {
    const std::optional<std::time_t> opt_mtime = Fmi::last_write_time(thePath);
    if (!opt_mtime)
    {
      Fmi::Exception err(BCP, "Failed to get last write time");
      err.addParameter("path", thePath);
      throw err;
    }
    std::time_t mtime = *opt_mtime;

    // Try using the cache with a lock first
    {
      ReadLock lock(itsMutex);
      auto iter = itsCache.find(thePath);
      if (iter != itsCache.end())
      {
        if (mtime == iter->second.modification_time)
          return iter->second.content;
      }
    }

    // No active lock while we read the file contents

    std::string content;
    std::ifstream in(thePath.c_str());
    if (!in)
      throw Fmi::Exception(BCP,
                           "Failed to open '" + std::string(thePath.c_str()) + "' for reading!");

    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    // Now insert the value into the cache and return it

    WriteLock lock(itsMutex);
    FileContents contents(mtime, content);
    itsCache[thePath] = contents;
    return content;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the file modification time, or 0 if the file does not exist
 */
// ----------------------------------------------------------------------

std::size_t FileCache::last_modified(const std::filesystem::path& thePath) const
{
  try
  {
    boost::system::error_code ec;
    std::optional<std::time_t> modtime = Fmi::last_write_time(thePath);
    if (modtime)
      return *modtime;
    return 0;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
