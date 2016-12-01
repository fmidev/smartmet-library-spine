// ======================================================================
/*!
 * \brief Interface of abstract class Formatter
 */
// ======================================================================

#pragma once
#include "HTTP.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class Table;
class TableFormatterOptions;

class TableFormatter
{
 public:
  typedef std::vector<std::string> Names;

  virtual ~TableFormatter();
  virtual void format(std::ostream& theOutput,
                      const Table& theTable,
                      const Names& theNames,
                      const HTTP::Request& theReq,
                      const TableFormatterOptions& theConfig) const = 0;

  virtual const std::string mimetype() const = 0;

};  // class TableFormatter

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
