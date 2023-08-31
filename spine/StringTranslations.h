#pragma once

#include "MultiLanguageString.h"
#include "MultiLanguageStringArray.h"
#include <libconfig.h++>
#include <boost/optional.hpp>
#include <map>
#include <string>

namespace SmartMet
{
namespace Spine
{
class StringTranslations
{
public:
  StringTranslations();
  StringTranslations(const libconfig::Config& config);

  void readTranslations(const libconfig::Config& config);
  void setDefaultLanguage(const std::string& theLanguage);
  const std::string& getDefaultLanguage() const;

  boost::optional<std::string> getStringTranslation(const std::string& theKey,
													const std::string& theLanguage) const;
  boost::optional<std::vector<std::string>> getStringArrayTranslation(const std::string& theKey,
																	  const std::string& theLanguage) const;

 private:
  std::map<std::string, MultiLanguageString> itsStringTranslations;
  std::map<std::string, MultiLanguageStringArray> itsStringArrayTranslations;
  std::string itsDefaultLanguage{"en"};
};
}  // namespace Spine
}  // namespace SmartMet
