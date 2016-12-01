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
  void format(std::ostream& theOutput,
              const Table& theTable,
              const TableFormatter::Names& theNames,
              const HTTP::Request& theReq,
              const TableFormatterOptions& theConfig) const;

  const std::string mimetype() const { return "text/html"; }
};
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
