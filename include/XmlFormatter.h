// ======================================================================
/*!
 * \brief Interface of class XmlFormatter
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class XmlFormatter : public TableFormatter
{
 public:
  void format(std::ostream& theOutput,
              const Table& theTable,
              const TableFormatter::Names& theNames,
              const HTTP::Request& theReq,
              const TableFormatterOptions& theConfig) const;

  const std::string mimetype() const { return "application/xml"; }
 private:
  void format_attributes(std::ostream& theOutput,
                         const Table& theTable,
                         const TableFormatter::Names& theNames,
                         const std::string& theTag,
                         const HTTP::Request& theReq) const;

  void format_tags(std::ostream& theOutput,
                   const Table& theTable,
                   const TableFormatter::Names& theNames,
                   const std::string& theTag,
                   const HTTP::Request& theReq) const;

  void format_mixed(std::ostream& theOutput,
                    const Table& theTable,
                    const TableFormatter::Names& theNames,
                    const std::string& theTag,
                    const HTTP::Request& theReq) const;
};
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
