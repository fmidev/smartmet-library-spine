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
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const;

  const std::string mimetype() const { return "application/xml"; }

 private:
  std::string format_attributes(const Table& theTable,
                                const TableFormatter::Names& theNames,
                                const std::string& theTag,
                                const HTTP::Request& theReq) const;

  std::string format_tags(const Table& theTable,
                          const TableFormatter::Names& theNames,
                          const std::string& theTag,
                          const HTTP::Request& theReq) const;

  std::string format_mixed(const Table& theTable,
                           const TableFormatter::Names& theNames,
                           const std::string& theTag,
                           const HTTP::Request& theReq) const;
};
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
