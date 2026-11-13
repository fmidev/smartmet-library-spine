#include "JsonCompare.h"
#include <fmt/format.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace SmartMet
{
namespace Spine
{
namespace JsonCompare
{
namespace
{
// Deeper documents are rejected instead of risking a stack overflow
constexpr int max_depth = 1000;

// ----------------------------------------------------------------------
// Parser
// ----------------------------------------------------------------------

class Parser
{
 public:
  explicit Parser(const std::string& text) : mText(text) {}

  std::optional<Node> parse()
  {
    Node node;
    skip_whitespace();
    if (!parse_value(node, 0))
      return {};
    skip_whitespace();
    if (mPos != mText.size())
      return {};
    return node;
  }

 private:
  const std::string& mText;
  std::size_t mPos = 0;

  bool at_end() const { return mPos >= mText.size(); }
  char peek() const { return at_end() ? '\0' : mText[mPos]; }

  void skip_whitespace()
  {
    while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r'))
      ++mPos;
  }

  bool consume(char ch)
  {
    if (peek() != ch)
      return false;
    ++mPos;
    return true;
  }

  bool is_digit() const { return !at_end() && std::isdigit(static_cast<unsigned char>(peek())); }

  bool skip_digits()
  {
    if (!is_digit())
      return false;
    while (is_digit())
      ++mPos;
    return true;
  }

  bool parse_value(Node& node, int depth)
  {
    if (depth > max_depth)
      return false;

    switch (peek())
    {
      case '{':
        return parse_object(node, depth);
      case '[':
        return parse_array(node, depth);
      case '"':
        node.type = Node::Type::String;
        return parse_string(node.text);
      case 't':
        node.type = Node::Type::Boolean;
        return parse_literal(node.text, "true");
      case 'f':
        node.type = Node::Type::Boolean;
        return parse_literal(node.text, "false");
      case 'n':
        node.type = Node::Type::Null;
        return parse_literal(node.text, "null");
      default:
        node.type = Node::Type::Number;
        return parse_number(node.text);
    }
  }

  bool parse_literal(std::string& text, const std::string& literal)
  {
    if (mText.compare(mPos, literal.size(), literal) != 0)
      return false;
    mPos += literal.size();
    text = literal;
    return true;
  }

  // -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
  bool parse_number(std::string& text)
  {
    const std::size_t start = mPos;
    consume('-');
    if (!consume('0') && !skip_digits())
      return false;
    if (consume('.') && !skip_digits())
      return false;
    if (consume('e') || consume('E'))
    {
      if (!consume('+'))
        consume('-');
      if (!skip_digits())
        return false;
    }
    text = mText.substr(start, mPos - start);
    return true;
  }

  // The string is kept as written, including the quotes and the escapes
  bool parse_string(std::string& text)
  {
    const std::size_t start = mPos;
    if (!consume('"'))
      return false;

    while (true)
    {
      if (at_end())
        return false;
      const auto ch = static_cast<unsigned char>(mText[mPos++]);
      if (ch == '"')
        break;
      if (ch < 0x20)
        return false;
      if (ch == '\\')
      {
        if (at_end())
          return false;
        const char esc = mText[mPos++];
        if (esc == 'u')
        {
          for (int i = 0; i < 4; i++)
          {
            if (at_end() || !std::isxdigit(static_cast<unsigned char>(mText[mPos++])))
              return false;
          }
        }
        else if (std::string("\"\\/bfnrt").find(esc) == std::string::npos)
          return false;
      }
    }

    text = mText.substr(start, mPos - start);
    return true;
  }

  bool parse_array(Node& node, int depth)
  {
    node.type = Node::Type::Array;
    consume('[');
    skip_whitespace();
    if (consume(']'))
      return true;

    while (true)
    {
      skip_whitespace();
      node.children.emplace_back();
      if (!parse_value(node.children.back(), depth + 1))
        return false;
      skip_whitespace();
      if (consume(']'))
        return true;
      if (!consume(','))
        return false;
    }
  }

  bool parse_object(Node& node, int depth)
  {
    node.type = Node::Type::Object;
    consume('{');
    skip_whitespace();
    if (consume('}'))
      return true;

    while (true)
    {
      skip_whitespace();
      node.keys.emplace_back();
      if (!parse_string(node.keys.back()))
        return false;
      skip_whitespace();
      if (!consume(':'))
        return false;
      skip_whitespace();
      node.children.emplace_back();
      if (!parse_value(node.children.back(), depth + 1))
        return false;
      skip_whitespace();
      if (consume('}'))
        return true;
      if (!consume(','))
        return false;
    }
  }
};

// ----------------------------------------------------------------------
// Pretty printing
// ----------------------------------------------------------------------

void pretty(const Node& node, std::string& out, int indent)
{
  const auto newline = [&out](int n) { out += '\n' + std::string(2 * n, ' '); };

  if (node.type != Node::Type::Array && node.type != Node::Type::Object)
  {
    out += node.text;
    return;
  }

  const bool is_object = (node.type == Node::Type::Object);
  out += (is_object ? '{' : '[');
  for (std::size_t i = 0; i < node.children.size(); i++)
  {
    if (i > 0)
      out += ',';
    newline(indent + 1);
    if (is_object)
      out += node.keys[i] + ": ";
    pretty(node.children[i], out, indent + 1);
  }
  if (!node.children.empty())
    newline(indent);
  out += (is_object ? '}' : ']');
}

// ----------------------------------------------------------------------
// Comparison
// ----------------------------------------------------------------------

const char* type_name(Node::Type type)
{
  switch (type)
  {
    case Node::Type::Null:
      return "null";
    case Node::Type::Boolean:
      return "boolean";
    case Node::Type::Number:
      return "number";
    case Node::Type::String:
      return "string";
    case Node::Type::Array:
      return "array";
    case Node::Type::Object:
      return "object";
  }
  return "unknown";
}

// The value for messages: scalars as written (shortened), containers by type
std::string describe(const Node& node)
{
  if (node.type == Node::Type::Array || node.type == Node::Type::Object)
    return fmt::format("{} of size {}", type_name(node.type), node.children.size());

  const std::size_t max_length = 60;
  if (node.text.size() <= max_length)
    return node.text;
  return node.text.substr(0, max_length) + "...";
}

// The path of an object member: .name for plain names, otherwise ["name"]
std::string member_path(const std::string& path, const std::string& key)
{
  const std::string name = key.substr(1, key.size() - 2);
  const bool plain =
      !name.empty() && !std::isdigit(static_cast<unsigned char>(name[0])) &&
      std::all_of(name.begin(),
                  name.end(),
                  [](char ch)
                  { return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_'; });
  if (plain)
    return path + '.' + name;
  return path + '[' + key + ']';
}

void append_utf8(std::string& out, unsigned int cp)
{
  if (cp < 0x80)
    out += static_cast<char>(cp);
  else if (cp < 0x800)
  {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
  else if (cp < 0x10000)
  {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
  else
  {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

// The value of a parsed string (with quotes), so that different escapes of
// the same characters such as \" and " compare equal
std::string decode_string(const std::string& text)
{
  std::string out;
  const std::size_t end = text.size() - 1;
  std::size_t pos = 1;

  const auto hex4 = [&text](std::size_t p)
  { return static_cast<unsigned int>(std::stoul(text.substr(p, 4), nullptr, 16)); };

  while (pos < end)
  {
    const char ch = text[pos++];
    if (ch != '\\')
    {
      out += ch;
      continue;
    }

    const char esc = text[pos++];
    switch (esc)
    {
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      case 'u':
      {
        unsigned int cp = hex4(pos);
        pos += 4;
        // Surrogate pair
        if (cp >= 0xD800 && cp < 0xDC00 && pos + 6 <= end && text[pos] == '\\' &&
            text[pos + 1] == 'u')
        {
          const unsigned int low = hex4(pos + 2);
          if (low >= 0xDC00 && low < 0xE000)
          {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            pos += 6;
          }
        }
        append_utf8(out, cp);
        break;
      }
      default:  // " \ /
        out += esc;
    }
  }
  return out;
}

bool is_integer(const std::string& number)
{
  return number.find_first_of(".eE") == std::string::npos;
}

std::optional<std::string> compare(const Node& expected,
                                   const Node& result,
                                   double tolerance,
                                   const std::string& path)
{
  if (expected.type != result.type)
    return fmt::format("{}: expected {} {}, got {} {}",
                       path,
                       type_name(expected.type),
                       describe(expected),
                       type_name(result.type),
                       describe(result));

  switch (expected.type)
  {
    case Node::Type::Null:
    case Node::Type::Boolean:
      if (expected.text == result.text)
        return {};
      return fmt::format("{}: expected {}, got {}", path, describe(expected), describe(result));

    case Node::Type::String:
      if (expected.text == result.text ||
          decode_string(expected.text) == decode_string(result.text))
        return {};
      return fmt::format("{}: expected {}, got {}", path, describe(expected), describe(result));

    case Node::Type::Number:
      if (numbers_equal(expected.text, result.text, tolerance))
        return {};
      return fmt::format("{}: expected {}, got {}", path, expected.text, result.text);

    case Node::Type::Array:
    {
      const std::size_t n = std::min(expected.children.size(), result.children.size());
      for (std::size_t i = 0; i < n; i++)
      {
        auto diff = compare(
            expected.children[i], result.children[i], tolerance, fmt::format("{}[{}]", path, i));
        if (diff)
          return diff;
      }
      if (expected.children.size() == result.children.size())
        return {};
      return fmt::format("{}: expected array of size {}, got size {}",
                         path,
                         expected.children.size(),
                         result.children.size());
    }

    case Node::Type::Object:
    {
      const std::size_t n = std::min(expected.children.size(), result.children.size());
      for (std::size_t i = 0; i < n; i++)
      {
        if (expected.keys[i] != result.keys[i] &&
            decode_string(expected.keys[i]) != decode_string(result.keys[i]))
          return fmt::format("{}: expected member {}, got {}",
                             path,
                             describe(Node{Node::Type::String, expected.keys[i], {}, {}}),
                             describe(Node{Node::Type::String, result.keys[i], {}, {}}));
        auto diff = compare(expected.children[i],
                            result.children[i],
                            tolerance,
                            member_path(path, expected.keys[i]));
        if (diff)
          return diff;
      }
      if (expected.children.size() > n)
        return fmt::format("{}: missing member {}", path, expected.keys[n]);
      if (result.children.size() > n)
        return fmt::format("{}: unexpected member {}", path, result.keys[n]);
      return {};
    }
  }
  return {};
}

}  // namespace

// ----------------------------------------------------------------------

std::optional<Node> parse(const std::string& text)
{
  return Parser(text).parse();
}

std::string pretty(const Node& node)
{
  std::string out;
  pretty(node, out, 0);
  out += '\n';
  return out;
}

bool numbers_equal(const std::string& a, const std::string& b, double tolerance)
{
  if (a == b)
    return true;

  // Integers are not subject to rounding errors
  if (is_integer(a) && is_integer(b))
    return false;

  char* a_end = nullptr;
  char* b_end = nullptr;
  const double x = std::strtod(a.c_str(), &a_end);
  const double y = std::strtod(b.c_str(), &b_end);
  if (*a_end != '\0' || *b_end != '\0' || !std::isfinite(x) || !std::isfinite(y))
    return false;

  const double scale = std::max({1.0, std::abs(x), std::abs(y)});
  return std::abs(x - y) <= tolerance * scale;
}

std::optional<std::string> compare(const Node& expected, const Node& result, double tolerance)
{
  return compare(expected, result, tolerance, "$");
}

}  // namespace JsonCompare
}  // namespace Spine
}  // namespace SmartMet
