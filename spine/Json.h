// ======================================================================
/*!
 * \brief JSON tools
 */
//======================================================================

#pragma once
#include "HTTP.h"
#include <json/json.h>
#include <macgyver/Exception.h>
#include <macgyver/TimeParser.h>
#include <set>
#include <stdexcept>
#include <string>

namespace SmartMet
{
namespace Spine
{
class JsonCache;

namespace JSON
{
// void substitute json-includes and references from query string options
void replaceReferences(Json::Value& theJson,
                       const HTTP::ParamMap& theParams,
                       const std::string& thePrefix = "",
                       bool theCaseIsInsensitive = true);

// expand includes in the Json ("json:file/name.json")
void preprocess(Json::Value& theJson,
                const std::string& theRootPath,
                const std::string& thePath,
                const JsonCache& theJsonCache);

// expand references in the Json ("path:name1.name2[0].parameter")
void dereference(Json::Value& theJson);

// expand normal query string options
void expand(Json::Value& theJson,
            const HTTP::ParamMap& theParams,
            const std::string& thePrefix = "",
            bool theCaseIsInsensitive = true);

// ----------------------------------------------------------------------

// extract a set of strings
void extract_set(const std::string& theName,
                 std::set<std::string>& theSet,
                 const Json::Value& theJson);

// extract a set of integers
void extract_set(const std::string& theName, std::set<int>& theSet, const Json::Value& theJson);

// extract an array
template <typename Container, typename Conf>
void extract_array(const std::string& theName,
                   Container& theContainer,
                   const Json::Value& theJson,
                   const Conf& theConf)
{
  try
  {
    if (!theJson.isArray())
      throw Fmi::Exception(BCP, theName + " setting must be an array");

    for (unsigned int i = 0; i < theJson.size(); i++)
    {
      const Json::Value& json = theJson[i];
      typename Container::value_type value;
      value.init(json, theConf);
      theContainer.push_back(value);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace JSON
}  // namespace Spine
}  // namespace SmartMet
