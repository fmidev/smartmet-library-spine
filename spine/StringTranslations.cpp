#include "StringTranslations.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <boost/filesystem.hpp>
#include <json/reader.h>
#include <iostream>


namespace SmartMet
{
namespace Spine
{

StringTranslations::StringTranslations()
{
}

StringTranslations::StringTranslations(const libconfig::Config& config)
{
  readTranslations(config);
}

void StringTranslations::setDefaultLanguage(const std::string& theLanguage)
{
  itsDefaultLanguage = theLanguage;
}

const std::string& StringTranslations::getDefaultLanguage() const
{
  return itsDefaultLanguage;
}

void StringTranslations::readTranslations(const libconfig::Config& theConfig)
{
  try
  {
    Json::Reader jsonreader;

    // Establish default language

    std::string language = itsDefaultLanguage;
    if (theConfig.exists("language"))
	  theConfig.lookupValue("language", language);

	//	setDefaultLanguage(language);

    // Read all parameter translations. We assume JSON encoded strings to avoid config file
    // encoding ambiguities. libconfig itself provides no extra Unicode support.

    if (!theConfig.exists("string_translations"))
      return;

    const libconfig::Setting& settings = theConfig.lookup("string_translations");


    if (!settings.isGroup())
      throw Fmi::Exception(
          BCP, "string translations must be a group of string name to translations mappings");

    for (int i = 0; i < settings.getLength(); i++)
    {
      const auto& string_settings = settings[i];
      std::string string_name = Fmi::ascii_tolower_copy(string_settings.getName());
	  if (!string_settings.isGroup())
      throw Fmi::Exception(
          BCP, string_name + " must be a group of translations");

	  for (int k = 0; k < string_settings.getLength(); k++)
		{
		  auto& language_settings = string_settings[k];
		  auto& parent_settings = language_settings.getParent();
		  if(!language_settings.isArray())
			{
			  itsStringTranslations[string_name] = MultiLanguageString(itsDefaultLanguage, parent_settings);
			}
		  else
			{
			  itsStringArrayTranslations[string_name] = MultiLanguageStringArray(itsDefaultLanguage, parent_settings);
			}
		}	  
	}
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Spine::ParameterTranslations::readTranslations failed!");
  }
}

std::optional<std::string> StringTranslations::getStringTranslation(const std::string& theKey,
																	  const std::string& theLanguage) const
{
  if(itsStringTranslations.find(theKey) == itsStringTranslations.end())
	return {};

  const auto& string_translations = itsStringTranslations.at(theKey);
  
  const auto& content = string_translations.get_content();

  if(content.find(theLanguage) == content.end())
	return {};

  return content.at(theLanguage);
}

std::optional<std::vector<std::string>> StringTranslations::getStringArrayTranslation(const std::string& theKey,
																						const std::string& theLanguage) const
{
  if(itsStringArrayTranslations.find(theKey) == itsStringArrayTranslations.end())
	return {};

  const auto& string_array_translations = itsStringArrayTranslations.at(theKey);
  
  const auto& content = string_array_translations.get_content();

  if(content.find(theLanguage) == content.end())
	return {};

  return content.at(theLanguage);
}


}  // namespace Spine
}  // namespace SmartMet

