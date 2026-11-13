// ======================================================================
/*!
 * \brief JSON aware comparison of test results
 *
 * Compares JSON documents structurally: whitespace is ignored, numbers
 * are compared with a tolerance, everything else (including member order
 * and strings which happen to contain numbers) must match exactly. Strings
 * are compared by value, so different escapes of the same characters
 * (such as \" and \u0022) are equal.
 *
 * The parser keeps the members of objects in their original order and
 * scalars as their original text, so the pretty printed output differs
 * from the input only by whitespace.
 */
// ======================================================================

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace SmartMet
{
namespace Spine
{
namespace JsonCompare
{
struct Node
{
  enum class Type
  {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
  };

  Type type = Type::Null;
  std::string text;               // the original text of scalars (strings with quotes)
  std::vector<std::string> keys;  // the original texts of object keys (with quotes)
  std::vector<Node> children;     // array elements or object member values
};

// Parse a JSON document. Returns nothing if the text is not valid JSON.
std::optional<Node> parse(const std::string& text);

// Pretty print with 2 space indentation
std::string pretty(const Node& node);

// Are the numbers (as JSON text) equal within the tolerance? Integers must be
// equal exactly, otherwise |a-b| <= tolerance * max(1, |a|, |b|): a relative
// tolerance which becomes absolute for numbers smaller than 1 in magnitude.
bool numbers_equal(const std::string& a, const std::string& b, double tolerance);

// Compare a result with the expected one. Returns nothing if they are equal,
// otherwise the location (as JSONPath) and description of the first difference.
std::optional<std::string> compare(const Node& expected, const Node& result, double tolerance);

}  // namespace JsonCompare
}  // namespace Spine
}  // namespace SmartMet
