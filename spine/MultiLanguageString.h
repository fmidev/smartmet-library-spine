#pragma once

#include <libconfig.h++>
#include <map>
#include <memory>
#include <string>

namespace SmartMet
{
namespace Spine
{
class MultiLanguageString
{
 public:
  MultiLanguageString(const std::string& default_language, libconfig::Setting& setting);
  MultiLanguageString() {}

  static std::shared_ptr<MultiLanguageString> create(const std::string& default_language,
                                                     libconfig::Setting& setting);

  virtual ~MultiLanguageString();

  inline std::string get_default_language() const { return default_language; }
  inline std::string get() const { return get(default_language); }
  std::string get(const std::string& language) const;

  inline const std::map<std::string, std::string>& get_content() const { return data; }

 private:
  std::string default_language;
  std::map<std::string, std::string> data;
};

using MultiLanguageStringP = std::shared_ptr<MultiLanguageString>;

}  // namespace Spine
}  // namespace SmartMet
