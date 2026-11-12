// ======================================================================
/*!
 * \brief Interface of class XmlFormatter
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

class XmlFormatter : public TableFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const override;

  std::string mimetype() const override { return "application/xml"; }

 private:
  std::string format_attributes(const Table& theTable,
                                const TableFormatter::Names& theNames,
                                const std::string& theTag,
                                const HTTP::Request& theReq) const;

  std::string format_tags(const Table& theTable,
                          const TableFormatter::Names& theNames,
                          const std::string& theTag,
                          const HTTP::Request& theReq) const;

  std::string format_mixed(const Table& theTable,
                           const TableFormatter::Names& theNames,
                           const std::string& theTag,
                           const HTTP::Request& theReq) const;
};
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
