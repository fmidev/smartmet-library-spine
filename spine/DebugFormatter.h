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
};
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
