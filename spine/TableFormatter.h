// ======================================================================
/*!
 * \brief Interface of abstract class Formatter
 */
// ======================================================================

#pragma once
#include <iosfwd>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class Table;
class TableFormatterOptions;

namespace HTTP
{
class Request;
}

class TableFormatter
{
 public:
  using Names = std::vector<std::string>;

  virtual ~TableFormatter();
  virtual std::string format(const Table& theTable,
                             const Names& theNames,
                             const HTTP::Request& theReq,
                             const TableFormatterOptions& theConfig) const = 0;

  virtual const std::string mimetype() const = 0;

  static const std::size_t default_minimum_size = 8191;

};  // class TableFormatter

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
