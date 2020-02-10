#include "MultiLanguageString.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include "spine/Exception.h"
#include <sstream>

/**

@page SPINE_CFG_MULTI_LANGUAGE_STRING cfgMultiLanguageString

This configuration object contains string value in several langugaes.

The default language is specified when reading multilanguage string from
libconfig type configuration file. A translation to the default language
is required to be present

An example:

@verbatim
title: { eng: "An example"; fin: "Esimerkki"; lav:"Piemērs"; };
@endverbatim

One can alternatively specify a single string like:

@verbatim
title: "An example";
@endverbatim

In this case it is assumed that it is provided for the default language.

*/

namespace ba = boost::algorithm;
using SmartMet::Spine::MultiLanguageString;

MultiLanguageString::MultiLanguageString(
    const std::string& default_language,
    libconfig::Setting& setting)
    : default_language(Fmi::ascii_tolower_copy(default_language)), data()
{
  try
  {
    bool found_default = false;

    if (setting.getType() == libconfig::Setting::TypeString)
    {
      const std::string value = setting;
      const std::string name = default_language;
      data[name] = value;
    } else if (setting.getType() == libconfig::Setting::TypeGroup) {
      const int num_items = setting.getLength();
      for (int i = 0; i < num_items; i++)
	{
	  libconfig::Setting& item = setting[i];
	  if (item.getType() != libconfig::Setting::TypeString)
	    {
	      std::ostringstream msg;
	      msg << "Libconfig string expected instead of '";
	      SmartMet::Spine::ConfigBase::dump_setting(msg, item);
	      msg << "' while reading item '";
	      SmartMet::Spine::ConfigBase::dump_setting(msg, setting);
	      msg << "'";
	      throw SmartMet::Spine::Exception(BCP, msg.str());
	    }

	  const std::string value = item;
	  const std::string tmp = item.getName();
	  const std::string name = Fmi::ascii_tolower_copy(tmp);
	  if (name == this->default_language)
	    found_default = true;
	  data[name] = value;
	}

      if (not found_default)
	{
	  std::ostringstream msg;
	  msg << "The string for the default language '" << this->default_language
	      << "' is not found in '";
	  SmartMet::Spine::ConfigBase::dump_setting(msg, setting);
	  throw SmartMet::Spine::Exception(BCP, msg.str());
	}
    } else {
      std::ostringstream msg;
      msg << "Libconfig group expected instead of '";
      SmartMet::Spine::ConfigBase::dump_setting(msg, setting);
      msg << "'";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::shared_ptr<MultiLanguageString> MultiLanguageString::create(
    const std::string& default_language, libconfig::Setting& setting)
{
  try
  {
    std::shared_ptr<MultiLanguageString> result(
        new MultiLanguageString(default_language, setting));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

MultiLanguageString::~MultiLanguageString() {}

std::string MultiLanguageString::get(const std::string& language) const
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
