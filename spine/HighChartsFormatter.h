// ======================================================================
/*!
 * \brief Interface of class HighChartsFormatter
 */
// ======================================================================

#pragma once

#include "HtmlFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class HighChartsFormatter : public HtmlFormatter
{
 public:
  void format(std::ostream& theOutput,
              const Table& theTable,
              const TableFormatter::Names& theNames,
              const HTTP::Request& theReq,
              const TableFormatterOptions& theConfig) const;
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
