#include "Json.h"
#include "FileCache.h"
#include "HTTP.h"
#include "Exception.h"
#include <macgyver/String.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

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
 *    "projection":   "path:name1.name2.name3[0].projection",
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
 */
// ----------------------------------------------------------------------

void JSON::expand(Json::Value& theJson,
                  const HTTP::ParamMap& theParams,
                  const std::string& thePrefix,
                  bool theCaseIsInsensitive)
{
  try
  {
    Json::Reader reader;

    if (theJson.isArray())
    {
      for (unsigned int i = 0; i < theJson.size(); i++)
      {
        Json::Value& json = theJson[i];
        expand(json, theParams, thePrefix, theCaseIsInsensitive);
      }
    }
    else if (theJson.isObject())
    {
      // Establish the prefix
      std::string prefix = thePrefix;
      if (theJson.isMember("qid"))
      {
        const Json::Value& json = theJson.get("qid", prefix);
        if (!json.isString())
          throw SmartMet::Spine::Exception(BCP, "The 'qid' value must be a string!");
        prefix = json.asString();
      }

      // Override old values and add new ones

      for (const auto& name_value : theParams)
      {
        auto name = name_value.first;
        if (theCaseIsInsensitive)
          Fmi::ascii_tolower(name);

        bool match = false;

        if (prefix.empty())
        {
          // Put all input without dots to global level
          match = (name.find(".") == std::string::npos);
        }
        else
        {
          if (boost::algorithm::starts_with(name, prefix) &&
              (name.size() > prefix.size() && name[prefix.size()] == '.') &&
              name.find(".", prefix.size() + 1) == std::string::npos)
          {
            match = true;
            name = name.substr(prefix.size() + 1, std::string::npos);
          }
        }

        if (match)
        {
          if (!boost::regex_match(name_value.second, number_regex))
            theJson[name] = name_value.second;
          else
          {
            Json::Value value;
            // Try parsing as JSON, store as string on failure
            if (reader.parse(name_value.second, value, false))
              theJson[name] = value;
            else
              theJson[name] = name_value.second;
          }
        }
      }

      // Expand members
      const auto members = theJson.getMemberNames();
      for (const auto& name : members)
      {
        if (prefix.empty())
          expand(theJson[name], theParams, name);
        else
          expand(theJson[name], theParams, prefix + "." + name);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet
