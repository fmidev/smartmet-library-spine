#pragma once

#include <libconfig.h++>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class MultiLanguageStringArray
{
 public:
  MultiLanguageStringArray(const std::string& default_language, libconfig::Setting& setting);

  static std::shared_ptr<MultiLanguageStringArray> create(const std::string& default_language,
                                                          libconfig::Setting& setting);

  virtual ~MultiLanguageStringArray();

  inline std::string get_default_language() const { return default_language; }
  inline const std::vector<std::string>& get() const { return get(default_language); }

  const std::vector<std::string>& get(const std::string& language) const;

  inline const std::map<std::string, std::vector<std::string> >& get_content() const
  {
    return data;
  }

  using Ptr = std::shared_ptr<MultiLanguageStringArray>;

 private:
  void parse_content(libconfig::Setting& setting);

 private:
  const std::string default_language;
  std::map<std::string, std::vector<std::string> > data;
};
}  // namespace Spine
}  // namespace SmartMet
