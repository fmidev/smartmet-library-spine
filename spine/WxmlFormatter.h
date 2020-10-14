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
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const;

  const std::string mimetype() const { return "application/xml"; }

 private:
  std::string format_100(const Table& theTable,
                         const TableFormatter::Names& theNames,
                         const HTTP::Request& theReq,
                         const TableFormatterOptions& theConfig) const;

  std::string format_200(const Table& theTable,
                         const TableFormatter::Names& theNames,
                         const HTTP::Request& theReq,
                         const TableFormatterOptions& theConfig) const;
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
