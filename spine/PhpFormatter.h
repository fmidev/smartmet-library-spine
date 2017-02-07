// ======================================================================
/*!
 * \brief Interface of class PhpFormatter
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class PhpFormatter : public TableFormatter
{
 public:
  void format(std::ostream& theOutput,
              const Table& theTable,
              const TableFormatter::Names& theNames,
              const HTTP::Request& theReq,
              const TableFormatterOptions& theConfig) const;

  const std::string mimetype() const { return "text/plain"; }
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
