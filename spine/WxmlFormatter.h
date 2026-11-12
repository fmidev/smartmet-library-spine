// ======================================================================
/*!
 * \brief Interface of class WxmlFormatter
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"
#include <string>

namespace SmartMet::Spine::HTTP
{
class Request;
}  // namespace SmartMet::Spine::HTTP
namespace SmartMet::Spine
{
class Table;
}  // namespace SmartMet::Spine

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
