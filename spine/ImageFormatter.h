// ======================================================================
/*!
 * \brief Interface of class ImageFormatter
 */
// ======================================================================

#pragma once

#include "DebugFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class ImageFormatter : public DebugFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const override;
};
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
