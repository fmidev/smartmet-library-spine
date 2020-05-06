#include "MultiLanguageStringArray.h"
#include "ConfigBase.h"
#include "Exception.h"
#include <macgyver/StringConversion.h>

using SmartMet::Spine::MultiLanguageStringArray;

/**

@page SPINE_CFG_MULTI_LANGUAGE_STRING_ARRAY cfgMultiLanguageStringArray

This configuration object contains values of string array in several langugaes.

The default language is specified when reading multilanguage string array rom
libconfig type configuration file. A translation to the default language
is required to be present. The size of the array is required to be identical for
all provided languages.

An example:

@verbatim
numbers:
{
    eng: [ "one", "two", "three", "four" ];
    fin: [ "yksi", "kaksi", "kolme", "neljä" ];
    lav: [ "viens", "divi", "trīs", "četri" ];
};
@endverbatim

One can alternatively specify a single string array like:

@verbatim
numbers: [ "one", "two", "three", "four" ];
@endverbatim

In this case it is assumed that strings are provided for the default language.

*/

MultiLanguageStringArray::MultiLanguageStringArray(const std::string& default_language,
                                                   libconfig::Setting& setting)

    : default_language(default_language), data()
{
  try
  {
    parse_content(setting);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::shared_ptr<MultiLanguageStringArray> MultiLanguageStringArray::create(
    const std::string& default_language, libconfig::Setting& setting)
{
  try
  {
    std::shared_ptr<MultiLanguageStringArray> result(
        new MultiLanguageStringArray(default_language, setting));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

MultiLanguageStringArray::~MultiLanguageStringArray() {}

const std::vector<std::string>& MultiLanguageStringArray::get(const std::string& language) const
{
  try
  {
    auto pos = data.find(Fmi::ascii_tolower_copy(language));
    if (pos == data.end())
    {
      return get(default_language);
    }
    else
    {
      return pos->second;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void MultiLanguageStringArray::parse_content(libconfig::Setting& setting)
{
  int size = -1;
  bool found_default = false;

  if (setting.getType() != libconfig::Setting::TypeGroup)
  {
    std::ostringstream msg;
    msg << "Libconfig group expected instead of '";
    SmartMet::Spine::ConfigBase::dump_setting(msg, setting);
    msg << "'";
    throw SmartMet::Spine::Exception(BCP, msg.str());
  }

  const int num_items = setting.getLength();
  for (int i = 0; i < num_items; i++)
  {
    libconfig::Setting& item = setting[i];
    if (item.getType() != libconfig::Setting::TypeArray)
    {
      std::ostringstream msg;
      msg << "Libconfig array expected instead of '";
      SmartMet::Spine::ConfigBase::dump_setting(msg, item);
      msg << "' while reading item '";
      SmartMet::Spine::ConfigBase::dump_setting(msg, setting);
      msg << "'";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    std::vector<std::string> value;
    const int num_strings = item.getLength();
    for (int i = 0; i < num_strings; i++)
    {
      const std::string v = item[i];
      value.push_back(v);
    }

    if (i == 0)
    {
      size = num_strings;
    }
    else if (size != num_strings)
    {
      std::ostringstream msg;
      msg << "All language versions of string array are expected to have the same size"
          << " in '";
      SmartMet::Spine::ConfigBase::dump_setting(msg, setting);
      msg << '\'';
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }

    const std::string tmp = item.getName();
    const std::string name = Fmi::ascii_tolower_copy(tmp);
    if (name == this->default_language)
      found_default = true;
    data[name] = value;
  }

  if (not found_default)
  {
    std::ostringstream msg;
    msg << "The string array for the default language '" << this->default_language
        << "' is not found in '";
    SmartMet::Spine::ConfigBase::dump_setting(msg, setting);
    throw SmartMet::Spine::Exception(BCP, msg.str());
  }
}
