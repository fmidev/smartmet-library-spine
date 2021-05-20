// ======================================================================

#include "FileCache.h"
#include <boost/filesystem/operations.hpp>
#include <macgyver/Exception.h>
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

std::string FileCache::get(const boost::filesystem::path& thePath) const
{
  try
  {
    std::time_t mtime = boost::filesystem::last_write_time(thePath);

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

std::size_t FileCache::last_modified(const boost::filesystem::path& thePath) const
{
  try
  {
    boost::system::error_code ec;
    std::time_t modtime = boost::filesystem::last_write_time(thePath, ec);
    if (ec.value() == boost::system::errc::success)
      return modtime;
    return 0;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
