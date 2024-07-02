#pragma once

#include "ParameterTranslations.h"
#include "StringTranslations.h"
#include <libconfig.h++>
#include <optional>
#include <map>
#include <string>

namespace SmartMet
{
namespace Spine
{


class Translations
{
 public:
  Translations();
  Translations(const libconfig::Config& config);

  void readTranslations(const libconfig::Config& config);
  void setDefaultLanguage(const std::string& theDefaultLanguage);
  const std::string& getDefaultLanguage() const;

  std::optional<std::string> getParameterTranslation(const std::string& theParam,
													   int theValue,
													   const std::string& theLanguage) const;
  std::optional<std::string> getStringTranslation(const std::string& theKey,
													const std::string& theLanguage) const;
  std::optional<std::vector<std::string>> getStringArrayTranslation(const std::string& theKey,
																	  const std::string& theLanguage) const;

 private:
  ParameterTranslations itsParameterTranslations;
  StringTranslations itsStringTranslations;
};

}  // namespace Spine
}  // namespace SmartMet
