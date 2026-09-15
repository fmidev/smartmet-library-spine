// ======================================================================
/*!
 * \brief Interface of class JsonFormatter
 */
// ======================================================================

#pragma once
#include "Table.h"
#include "TableFormatter.h"
#include <list>

namespace SmartMet
{
namespace Spine
{
class TableFormatterOptions;

class JsonFormatter : public TableFormatter
{
 public:
  std::string format(const Table& theTable,
                     const TableFormatter::Names& theNames,
                     const HTTP::Request& theReq,
                     const TableFormatterOptions& theConfig) const override;

  std::string mimetype() const override { return "application/json"; }

 private:
  static std::string format_recursively(const Table& theTable,
                                        const Names& theNames,
                                        const HTTP::Request& theReq,
                                        Table::Indexes& theCols,
                                        const Table::Indexes& theRows,
                                        std::list<std::string>& theAttributes);
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
