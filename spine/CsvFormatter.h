// ======================================================================
/*!
 * \brief Interface of class CsvFormatter
 */
// ======================================================================

#pragma once
#include "TableFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class CsvFormatter : public TableFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const override;

  const std::string mimetype() const override { return "text/csv"; }
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
