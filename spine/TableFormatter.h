// ======================================================================
/*!
 * \brief Interface of abstract class Formatter
 */
// ======================================================================

#pragma once
#include <iosfwd>
#include <optional>
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

  virtual std::string mimetype() const = 0;

  static const std::size_t default_minimum_size = 8191;

 protected:
  // ----------------------------------------------------------------------
  /*!
   * \brief Test if the string looks like a number
   *
   * Note that the special values NaN and Inf accepted by Boost.Spirit in
   * various spellings are not considered to be numbers, since none of the
   * formats is able to represent them as one. This also keeps strings which
   * merely look like them intact, for example the name of the Thai station
   * "Nan".
   */
  // ----------------------------------------------------------------------

  static bool looks_number(const std::string& theValue);

  // ----------------------------------------------------------------------
  /*!
   * \brief Test if the string looks like a number and normalize it
   *
   * Boost.Spirit accepts several forms the JSON number grammar does not:
   *
   *     - a leading plus sign:        "+1"       --> "1"
   *     - leading zeroes:             "007"      --> "7"
   *     - a missing integer part:     ".12345"   --> "0.12345"
   *     - a missing fraction part:    "5."       --> "5"
   *
   * Return the value in the normalized form, or an empty optional if it is not
   * a number as defined by looks_number. Formats which do not restrict the form
   * of a number should use looks_number instead and output the value as is.
   */
  // ----------------------------------------------------------------------

  static std::optional<std::string> check_number(const std::string& theValue);

};  // class TableFormatter

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
