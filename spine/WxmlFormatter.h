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
                     const TableFormatterOptions& theConfig) const override;

  std::string mimetype() const override { return "application/xml"; }

 private:
  // Wxml is able to represent the special values, unlike the base class method
  static bool looks_number_or_special(const std::string& theValue);

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
