// ======================================================================
/*!
 * \brief Generic file content cache
 */
// ======================================================================

#pragma once

#include "Thread.h"
#include <filesystem>
#include <json/json.h>
#include <string>
#include <unordered_map>

namespace SmartMet
{
namespace Spine
{
class JsonCache
{
 public:
  // Always return a copy
  Json::Value get(const std::filesystem::path& thePath) const;

 private:
  struct Data
  {
    std::time_t modification_time = 0;
    Json::Value json;

    Data() = default;
    Data(std::time_t theTime, Json::Value theJson);
  };

  using Cache = std::unordered_map<std::size_t, Data>;
  mutable MutexType itsMutex;
  mutable Cache itsCache;

};  // class JsonCache

}  // namespace Spine
}  // namespace SmartMet
