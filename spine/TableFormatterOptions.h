// ======================================================================
/*!
 * \brief TableFormatter options
 */
// ======================================================================

#pragma once

#include <libconfig.h++>
#include <string>

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions
{
 public:
  TableFormatterOptions();
  explicit TableFormatterOptions(const libconfig::Config& theConfig);

  void setFormatType(const std::string& type) { itsFormatType = type; }
  const std::string& defaultWxmlTimeString() const { return itsDefaultWxmlTimeString; }
  const std::string& defaultWxmlVersion() const { return itsDefaultWxmlVersion; }
  const std::string& wxmlSchema() const { return itsWxmlSchema; }
  const std::string& xmlTag() const { return itsDefaultXmlTag; }
  const std::string& formatType() const { return itsFormatType; }

 private:
  std::string itsDefaultWxmlVersion;
  std::string itsDefaultWxmlTimeString;
  std::string itsWxmlSchema;
  std::string itsDefaultXmlTag;
  std::string itsFormatType;

};  // class TableFormatterOptions

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
