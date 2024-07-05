#include "ParameterTranslations.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <filesystem>
#include <json/reader.h>
#include <iostream>

namespace SmartMet
{
namespace Spine
{

ParameterTranslations::ParameterTranslations()
{
}

ParameterTranslations::ParameterTranslations(const libconfig::Config& config)
{
  readTranslations(config);
}

void ParameterTranslations::setDefaultLanguage(const std::string& theLanguage)
{
  itsDefaultLanguage = theLanguage;
}

const std::string& ParameterTranslations::getDefaultLanguage() const
{
  return itsDefaultLanguage;
}

void ParameterTranslations::addTranslation(const std::string& theParam,
                                           int theValue,
                                           const std::string& theLanguage,
                                           const std::string& theTranslation)
{
  auto theParamName = Fmi::ascii_tolower_copy(theParam);
  itsTranslations[theParamName][theValue][theLanguage] = theTranslation;
}

std::optional<std::string> ParameterTranslations::getTranslation(
    const std::string& theParam, int theValue, const std::string& theLanguage) const
{
  auto theParamName = Fmi::ascii_tolower_copy(theParam);
  auto param = itsTranslations.find(theParamName);
  if (param == itsTranslations.end())
    return {};

  auto lang = param->second.find(theValue);
  if (lang == param->second.end())
    return {};

  auto trans = lang->second.find(theLanguage);
  if (trans != lang->second.end())
    return trans->second;

  // No translation found for the requested language, use default language instead
  trans = lang->second.find(itsDefaultLanguage);
  if (trans == lang->second.end())
    return {};

  return trans->second;
}

void ParameterTranslations::readTranslations(const libconfig::Config& config)
{
  try
  {
    Json::Reader jsonreader;

    // Establish default language

    std::string language;
    if (config.lookupValue("language", language))
      setDefaultLanguage(language);

    // Read all parameter translations. We assume JSON encoded strings to avoid config file
    // encoding ambiguities. libconfig itself provides no extra Unicode support.

	auto translations_tag = "parameter_translations";
    if (!config.exists(translations_tag))
	  {
		// For backward compatibility also 'translations' tag is accepted
		translations_tag = "translations";
		if (!config.exists(translations_tag))
		  {
			// No parameter translations found
			return;
		  }
	  }

    const libconfig::Setting& settings = config.lookup(translations_tag);

    if (!settings.isGroup())
      throw Fmi::Exception(
          BCP, "translations must be a group of parameter name to translations mappings");

    for (int i = 0; i < settings.getLength(); i++)
    {
      const auto& param_settings = settings[i];
      if (!param_settings.isList())
        throw Fmi::Exception(BCP,
                             "translations must be lists of groups consisting of parameter value "
                             "and its translations");

      std::string param_name = Fmi::ascii_tolower_copy(param_settings.getName());
	  

      for (int j = 0; j < param_settings.getLength(); j++)
      {
        const auto& value_translations = param_settings[j];

        if (value_translations.isList())
          throw Fmi::Exception(BCP,
                               "translations for parameter " + param_name +
                                   " must be a list of translations for individual values");

        int param_value;
        if (!value_translations.lookupValue("value", param_value))
          throw Fmi::Exception(BCP,
                               "translation setting for " + param_name + " at position " +
                                   std::to_string(j) + " has no parameter value to be translated");

        for (int k = 0; k < value_translations.getLength(); k++)
        {
          const auto& translation = value_translations[k];

          std::string lang = translation.getName();
          if (lang == "value")
            continue;

          auto text = std::string("\"") + translation.c_str() + "\"";
          Json::Value json;
          bool ok = jsonreader.parse(text, json);
          if (!ok || !json.isString())
            throw Fmi::Exception(BCP, "Failed to parse JSON string '" + text + "'");

          addTranslation(param_name, param_value, lang, json.asString());
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Spine::ParameterTranslations::readTranslations");
  }
}


}  // namespace Spine
}  // namespace SmartMet
