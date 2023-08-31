#include "Translations.h"
#include <iostream>

namespace SmartMet
{
namespace Spine
{

Translations::Translations()
{
}

Translations::Translations(const libconfig::Config& config)
{
  readTranslations(config);
}

void Translations::readTranslations(const libconfig::Config& config)
{
  itsParameterTranslations.readTranslations(config);
  itsStringTranslations.readTranslations(config);
}

void Translations::setDefaultLanguage(const std::string& theDefaultLanguage)
{
  itsParameterTranslations.setDefaultLanguage(theDefaultLanguage);
  itsStringTranslations.setDefaultLanguage(theDefaultLanguage);
}

const std::string& Translations::getDefaultLanguage() const
{
  return itsParameterTranslations.getDefaultLanguage();
}

boost::optional<std::string> Translations::getParameterTranslation(const std::string& theParam,
																   int theValue,
																   const std::string& theLanguage) const
{
  return itsParameterTranslations.getTranslation(theParam, theValue, theLanguage);
}


boost::optional<std::string> Translations::getStringTranslation(const std::string& theKey,
																const std::string& theLanguage) const
{
  return itsStringTranslations.getStringTranslation(theKey, theLanguage);
}

boost::optional<std::vector<std::string>> Translations::getStringArrayTranslation(const std::string& theKey,
																				  const std::string& theLanguage) const
{
  return itsStringTranslations.getStringArrayTranslation(theKey, theLanguage);
}

}  // namespace Spine
}  // namespace SmartMet
