#include "Json.h"
#include "Exception.h"
#include "FileCache.h"
#include "HTTP.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Spine
{
// jsoncpp number parser sucks, it accepts things like YYYY-MM-DD and A,B,C,D
// See diagram in http://www.regexplained.co.uk/

// Note: This is global, since each regex has its own locale instance.
// Constructing the regex thousands of times can cause race conditions
// due to global locks in std::locale

// Matches start of object, array or string or a valid number
boost::regex number_regex(
    R"END(^((\{|\[|\").*)|([+-]?([0-9]*\.?[0-9]+|[0-9]+\.?[0-9]*)([eE][+-]?[0-9]+)?)$)END");

// ----------------------------------------------------------------------
/*!
 * \brief Extract QID part from a query string parameter
 */
// ----------------------------------------------------------------------

std::string extract_qid(const std::string& theName)
{
  auto dotpos = theName.find('.');
  if (dotpos == std::string::npos)
    return theName;

  return theName.substr(0, dotpos);
}

// ----------------------------------------------------------------------
/*!
 * \brief Collect all qid:s from Json
 */
// ----------------------------------------------------------------------

// map from qid to respective JSON object
using QidMap = std::map<std::string, Json::Value*>;

void collect_qids(Json::Value& theJson, QidMap& theQids, const std::string& thePrefix)
{
  try
  {
    if (theJson.isArray())
    {
      for (unsigned int i = 0; i < theJson.size(); i++)
        collect_qids(theJson[i], theQids, thePrefix + "[" + Fmi::to_string(i) + "]");
    }
    else if (theJson.isObject())
    {
      // Save path to qid if it is present
      if (theJson.isMember("qid"))
      {
        Json::Value& json = theJson["qid"];

        if (!json.isString())
          throw SmartMet::Spine::Exception(BCP, "The 'qid' value must be a string!");
        auto qid = json.asString();

        if (qid.empty())
          throw SmartMet::Spine::Exception(BCP, "The 'qid' value must not be empty!");

        auto dotpos = qid.find(".");
        if (dotpos != std::string::npos)
          throw SmartMet::Spine::Exception(BCP, "The 'qid' value must not contain dots!");

        theQids[qid] = &theJson;
      }

      // Search member variables

      const auto members = theJson.getMemberNames();
      for (const auto& name : members)
      {
        if (thePrefix.empty())
          collect_qids(theJson[name], theQids, name);
        else
          collect_qids(theJson[name], theQids, thePrefix + "." + name);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Failed to collect qid:s from JSON!", NULL);
  }
}

QidMap collect_qids(Json::Value& theJson)
{
  QidMap qids;
  collect_qids(theJson, qids, "");
  return qids;
}

// ----------------------------------------------------------------------
/*!
 * \brief Expand include statements in the JSON.
 *
 * For example:
 *
 * {
 *    "isobands":   "json:isobands/temperature.json",
 *    ...
 * }
 *
 * will be expanded so that the value of the isobands variable
 * is changed to the Json contents of the referenced file.
 * If the path begins with "/", expansion is done with respect
 * to the root path instead of the normal path.
 */
// ----------------------------------------------------------------------

void JSON::preprocess(Json::Value& theJson,
                      const std::string& theRootPath,
                      const std::string& thePath,
                      const FileCache& theFileCache)
{
  try
  {
    if (theJson.isString())
    {
      std::string tmp = theJson.asString();
      if (boost::algorithm::starts_with(tmp, "json:"))
      {
        std::string json_file;
        if (tmp.substr(5, 1) != "/")
          json_file = thePath + "/" + tmp.substr(5, std::string::npos);
        else
          json_file = theRootPath + "/" + tmp.substr(6, std::string::npos);

        Json::Reader reader;
        std::string json_text = theFileCache.get(json_file);
        // parse directly over old contents
        bool json_ok = reader.parse(json_text, theJson);
        if (!json_ok)
          throw SmartMet::Spine::Exception(
              BCP, "Failed to parse '" + json_file + "': " + reader.getFormattedErrorMessages());
        // TODO: should we prevent infinite recursion?
        preprocess(theJson, theRootPath, thePath, theFileCache);
      }
    }

    // Seek deeper in arrays
    else if (theJson.isArray())
    {
      for (unsigned int i = 0; i < theJson.size(); i++)
        preprocess(theJson[i], theRootPath, thePath, theFileCache);
    }
    // Seek deeper in objects
    else if (theJson.isObject())
    {
      const auto members = theJson.getMemberNames();
      BOOST_FOREACH (auto& name, members)
      {
        preprocess(theJson[name], theRootPath, thePath, theFileCache);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Expand references in the JSON.
 *
 * For example:
 *
 * {
 *    "projection":   "ref:name1.name2.name3[0].projection",
 *    ...
 * }
 *
 * will be expanded so that the value of the referenced variable
 * replaces the value of projection
 */
// ----------------------------------------------------------------------

void deref(Json::Value& theJson, Json::Value& theRoot)
{
  try
  {
    if (theJson.isString())
    {
      std::string tmp = theJson.asString();
      if (boost::algorithm::starts_with(tmp, "ref:"))
      {
        std::string path = "." + tmp.substr(4, std::string::npos);
        Json::Path json_path(path);
        Json::Value value = json_path.resolve(theRoot);
        if (value.isNull())
          throw SmartMet::Spine::Exception(BCP, "Failed to dereference '" + tmp + "'!");
        // We will not dereference the dereferenced value!
        theJson = value;
      }
    }

    // Seek deeper in arrays
    else if (theJson.isArray())
    {
      for (unsigned int i = 0; i < theJson.size(); i++)
        deref(theJson[i], theRoot);
    }
    // Seek deeper in objects
    else if (theJson.isObject())
    {
      const auto members = theJson.getMemberNames();
      BOOST_FOREACH (auto& name, members)
      {
        deref(theJson[name], theRoot);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void JSON::dereference(Json::Value& theJson)
{
  try
  {
    deref(theJson, theJson);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract a set of strings
 *
 * Allowed: "string" and ["string1" ... ]
 */
// ----------------------------------------------------------------------

void JSON::extract_set(const std::string& theName,
                       std::set<std::string>& theSet,
                       const Json::Value& theJson)
{
  try
  {
    if (theJson.isString())
      theSet.insert(theJson.asString());
    else if (theJson.isArray())
    {
      for (unsigned int i = 0; i < theJson.size(); i++)
      {
        const Json::Value& json = theJson[i];
        if (json.isString())
          theSet.insert(json.asString());
        else
          throw SmartMet::Spine::Exception(
              BCP, "The '" + theName + "' array must contain strings only!");
      }
    }
    else
      throw SmartMet::Spine::Exception(
          BCP, "The '" + theName + "' setting must be a string or an array of strings!");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract a set of integers
 *
 * Allowed: int and [int1 ... ]
 */
// ----------------------------------------------------------------------

void JSON::extract_set(const std::string& theName,
                       std::set<int>& theSet,
                       const Json::Value& theJson)
{
  try
  {
    if (theJson.isInt())
      theSet.insert(theJson.asInt());
    else if (theJson.isArray())
    {
      for (unsigned int i = 0; i < theJson.size(); i++)
      {
        const Json::Value& json = theJson[i];
        if (json.isInt())
          theSet.insert(json.asInt());
        else
          throw SmartMet::Spine::Exception(
              BCP, "The '" + theName + "' array must contain integers only");
      }
    }
    else
      throw SmartMet::Spine::Exception(
          BCP, "The '" + theName + "' setting must be an integer or an array of integers");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Expand Json from query string
 *
 * A map with "qid" set changes the current active prefix for query string
 * options. The top level qid is an empty string unless set in the json.
 *
 * Note: references are no longer substituted
 */
// ----------------------------------------------------------------------

void replaceFromQueryString(Json::Value& theJson,
                            const HTTP::ParamMap& theParams,
                            const std::string& thePrefix,
                            const QidMap& theQids,
                            bool theCaseIsInsensitive,
                            bool replaceReferences)
{
  try
  {
    // Needed for parsing JSON values in query strings
    Json::Reader reader;

    // Override old values and add new ones from query string

    for (const auto& name_value : theParams)
    {
      // WMS query string parameters are case insensitive, hence optional support is given
      auto name = name_value.first;
      if (theCaseIsInsensitive)
        Fmi::ascii_tolower(name);

      // Is the value a reference
      bool value_is_reference = (boost::algorithm::starts_with(name_value.second, "ref:") ||
                                 boost::algorithm::starts_with(name_value.second, "json:"));

      // Extract the value

      bool process_value =
          (replaceReferences && value_is_reference) || (!replaceReferences && !value_is_reference);

      // Skip the value if we do not wish to process it at this iteration (references or
      // non-references only)
      if (!process_value)
        continue;

      // Extract the JSON value from the query string
      Json::Value value;
      if (!boost::regex_match(name_value.second, number_regex))
        value = name_value.second;
      else
      {
        // Try parsing as JSON, store as string on failure
        if (!reader.parse(name_value.second, value, false))
          value = name_value.second;
      }

      // Assign the value to the proper location in the JSON

      auto path = name;              // Path for the setting
      auto qid = extract_qid(name);  // The part until the first dot

      // By default insertions go to top level
      Json::Value* json = &theJson;

      // Unless there is a matching qid
      auto qid_json = theQids.find(qid);
      if (qid_json != theQids.end())
      {
        if (qid == name)
          throw SmartMet::Spine::Exception(BCP, "Parameter name cannot match qid exactly")
              .addParameter("parameter", name)
              .addParameter("value", name_value.second);

        json = qid_json->second;

        path = path.substr(qid.size() + 1, std::string::npos);
      }

      // Now store the object following the remaining path.

      namespace ba = boost::algorithm;

      std::vector<std::string> path_components;
      ba::split(path_components, path, ba::is_any_of("."), ba::token_compress_on);

      if (path_components.size() == 1)
      {
        (*json)[path] = value;
      }
      else
      {
        for (const auto& component : path_components)
        {
          if (!json->isMember(component))
            (*json)[component] = Json::objectValue;
          json = &(*json)[component];
        }
        (*json) = value;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "JSON expansion failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Expand Json from query string
 *
 * A map with "qid" set changes the current active prefix for query string
 * options. The top level qid is an empty string unless set in the json.
 *
 * Note: references are no longer substituted
 */
// ----------------------------------------------------------------------

void JSON::expand(Json::Value& theJson,
                  const HTTP::ParamMap& theParams,
                  const std::string& thePrefix,
                  bool theCaseIsInsensitive)
{
  const bool replace_references = false;
  auto qids = collect_qids(theJson);
  replaceFromQueryString(
      theJson, theParams, thePrefix, qids, theCaseIsInsensitive, replace_references);
}

// ----------------------------------------------------------------------
/*!
 * \brief Replace references and includes given in query string
 *
 */
// ----------------------------------------------------------------------

void JSON::replaceReferences(Json::Value& theJson,
                             const HTTP::ParamMap& theParams,
                             const std::string& thePrefix,
                             bool theCaseIsInsensitive)
{
  const bool replace_references = true;
  auto qids = collect_qids(theJson);
  replaceFromQueryString(
      theJson, theParams, thePrefix, qids, theCaseIsInsensitive, replace_references);
}

}  // namespace Spine
}  // namespace SmartMet
