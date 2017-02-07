// ======================================================================
/*!
 * \brief Interface of class WxmlFormatter
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class WxmlFormatter : public TableFormatter
{
 public:
  void format(std::ostream& theOutput,
              const Table& theTable,
              const TableFormatter::Names& theNames,
              const HTTP::Request& theReq,
              const TableFormatterOptions& theConfig) const;

  const std::string mimetype() const { return "application/xml"; }
 private:
  void format_100(std::ostream& theOutput,
                  const Table& theTable,
                  const TableFormatter::Names& theNames,
                  const HTTP::Request& theReq,
                  const TableFormatterOptions& theConfig) const;

  void format_200(std::ostream& theOutput,
                  const Table& theTable,
                  const TableFormatter::Names& theNames,
                  const HTTP::Request& theReq,
                  const TableFormatterOptions& theConfig) const;
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
