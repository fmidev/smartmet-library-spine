// ======================================================================
/*!
 * \brief Interface of class JsonFormatter
 */
// ======================================================================

#pragma once
#include "TableFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class JsonFormatter : public TableFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const override;

  std::string mimetype() const override { return "application/json"; }
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
