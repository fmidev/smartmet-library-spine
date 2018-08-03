// ======================================================================
/*!
 * \brief Generic file content cache
 */
// ======================================================================

#pragma once

#include "Thread.h"
#include <boost/filesystem/path.hpp>
#include <json/json.h>
#include <map>
#include <string>

namespace SmartMet
{
namespace Spine
{
class JsonCache
{
 public:
  // Always return a copy
  Json::Value get(const boost::filesystem::path& thePath) const;

 private:
  struct Data
  {
    std::time_t modification_time;
    Json::Value json;

    Data() = default;
    Data(const std::time_t& theTime, const Json::Value& theJson);
  };

  using Cache = std::map<boost::filesystem::path, Data>;
  mutable MutexType itsMutex;
  mutable Cache itsCache;

};  // class JsonCache

}  // namespace Spine
}  // namespace SmartMet
