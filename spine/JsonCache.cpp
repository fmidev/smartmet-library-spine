// ======================================================================

#include "JsonCache.h"
#include <boost/filesystem/operations.hpp>
#include <boost/functional/hash.hpp>
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
 * \brief Construct JSON cache element
 */
// ----------------------------------------------------------------------

JsonCache::Data::Data(std::time_t theTime, Json::Value theJson)
    : modification_time(theTime), json(std::move(theJson))
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Get file JSON contents
 *
 * Note: Returns a deep copy
 */
// ----------------------------------------------------------------------

Json::Value JsonCache::get(const std::filesystem::path& thePath) const
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

    std::size_t hash = std::filesystem::hash_value(thePath);

    // Try using the cache with a lock first
    {
      ReadLock lock{itsMutex};
      auto iter = itsCache.find(hash);
      if (iter != itsCache.end())
      {
        if (mtime == iter->second.modification_time)
          return iter->second.json;
      }
    }

    // No active lock while we read the file contents

    std::string content;
    std::ifstream in{thePath.c_str()};
    if (!in)
      throw Fmi::Exception{BCP,
                           "Failed to open '" + std::string(thePath.c_str()) + "' for reading!"};

    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    // Parse the JSON

    Json::Reader reader;
    Json::Value json;
    bool json_ok = reader.parse(content, json);
    if (!json_ok)
      throw Fmi::Exception{
          BCP, "Failed to parse '" + thePath.string() + "': " + reader.getFormattedErrorMessages()};

    // Now insert the value into the cache and return it

    Data data{mtime, json};

    WriteLock lock{itsMutex};
    itsCache[hash] = data;
    return json;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}  // namespace Spine

}  // namespace Spine
}  // namespace SmartMet
