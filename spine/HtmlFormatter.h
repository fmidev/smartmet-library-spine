// ======================================================================
/*!
 * \brief Interface of class HtmlFormatter
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class HtmlFormatter : public TableFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const;

  const std::string mimetype() const { return "text/html"; }
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
