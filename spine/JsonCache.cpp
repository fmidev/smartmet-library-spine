// ======================================================================

#include "JsonCache.h"
#include "Exception.h"
#include <boost/filesystem/operations.hpp>
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

JsonCache::Data::Data(const std::time_t& theTime, const Json::Value& theJson)
    : modification_time(theTime), json(theJson)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Get file JSON contents
 *
 * Note: Returns a deep copy
 */
// ----------------------------------------------------------------------

Json::Value JsonCache::get(const boost::filesystem::path& thePath) const
{
  try
  {
    std::time_t mtime = boost::filesystem::last_write_time(thePath);

    // Try using the cache with a lock first
    {
      ReadLock lock{itsMutex};
      auto iter = itsCache.find(thePath);
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
      throw Spine::Exception{BCP,
                             "Failed to open '" + std::string(thePath.c_str()) + "' for reading!"};

    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    // Parse the JSON

    Json::Reader reader;
    Json::Value json;
    bool json_ok = reader.parse(content, json);
    if (!json_ok)
      throw Spine::Exception{
          BCP, "Failed to parse '" + thePath.string() + "': " + reader.getFormattedErrorMessages()};

    // Now insert the value into the cache and return it

    Data data{mtime, json};

    WriteLock lock{itsMutex};
    itsCache[thePath] = data;  // overwrites old contents
    return json;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}  // namespace Spine

}  // namespace Spine
}  // namespace SmartMet
