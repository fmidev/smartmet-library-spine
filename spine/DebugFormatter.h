// ======================================================================
/*!
 * \brief Interface of class DebugFormatter
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class DebugFormatter : public TableFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const override;

  std::string mimetype() const override { return "text/html"; }

 protected:
  std::string missing_value(const HTTP::Request& theReq) const;
  std::string make_header(const Table& theTable) const;
  std::string make_content(const Table& theTable, const HTTP::Request& theReq, bool escape) const;
};
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
