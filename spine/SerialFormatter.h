// ======================================================================
/*!
 * \brief Interface of class SerialFormatter
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class SerialFormatter : public TableFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const override;

  const std::string mimetype() const override { return "text/plain"; }
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
