// ======================================================================
/*!
 * \brief Interface of class SerialFormatter
 */
// ======================================================================

#pragma once

#include "Table.h"
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

  std::string mimetype() const override { return "text/plain"; }

 private:
  std::string format_plain(const Table& theTable,
                           const TableFormatter::Names& theNames,
                           const HTTP::Request& theReq,
                           const Table::Indexes& theCols,
                           const Table::Indexes& theRows) const;
  std::string format_attributes(const Table& theTable,
                                const TableFormatter::Names& theNames,
                                const HTTP::Request& theReq,
                                Table::Indexes& theCols,
                                const Table::Indexes& theRows,
                                std::list<std::string>& theAttributes) const;

  void appendNumber(std::string& theOutput, std::size_t theNumber) const;
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
