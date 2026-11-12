// ======================================================================
/*!
 * \brief Interface of class PhpFormatter
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"
#include <string>

namespace SmartMet
{
namespace Spine
{

class PhpFormatter : public TableFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const override;

  std::string mimetype() const override { return "text/plain"; }
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
