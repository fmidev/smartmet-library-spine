#pragma once

#include <libconfig.h++>
#include <optional>
#include <map>
#include <string>

namespace Json
{
class Reader;
}

namespace SmartMet
{
namespace Spine
{
class ParameterTranslations
{
public:
  
  ParameterTranslations();
  ParameterTranslations(const libconfig::Config& config);

  void readTranslations(const libconfig::Config& config);
  void setDefaultLanguage(const std::string& theLanguage);
  const std::string& getDefaultLanguage() const;

  std::optional<std::string> getTranslation(const std::string& theParam,
                                              int theValue,
                                              const std::string& theLanguage) const;

 private:
  void readValueTranslations(const libconfig::Setting& value_translations,
                             const std::string& param_name,
                             int position,
                             Json::Reader& jsonreader);

  void addTranslation(const std::string& theParam,
                      int theValue,
                      const std::string& theLanguage,
                      const std::string& theTranslation);

  using LanguageToWordMap = std::map<std::string, std::string>;
  using ValueToLanguageMap = std::map<int, LanguageToWordMap>;
  using ParameterToValueMap = std::map<std::string, ValueToLanguageMap>;

  ParameterToValueMap itsTranslations;
  std::string itsDefaultLanguage{"en"};
};
}  // namespace Spine
}  // namespace SmartMet
