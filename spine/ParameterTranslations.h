#pragma once

#include <libconfig.h++>
#include <boost/optional.hpp>
#include <map>
#include <string>

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

  boost::optional<std::string> getTranslation(const std::string& theParam,
                                              int theValue,
                                              const std::string& theLanguage) const;

 private:
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
