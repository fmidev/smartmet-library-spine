// ======================================================================

#include "TableFormatter.h"
#include <boost/spirit/include/qi.hpp>
#include <macgyver/Exception.h>
#include <cctype>

namespace qi = boost::spirit::qi;

namespace SmartMet
{
namespace Spine
{
// Defined here to avoid a weak vtable
TableFormatter::~TableFormatter() = default;

// ----------------------------------------------------------------------
/*!
 * \brief Test if the string looks like a number
 */
// ----------------------------------------------------------------------

bool TableFormatter::looks_number(const std::string& theValue)
{
  try
  {
    auto begin = theValue.cbegin();
    const auto end = theValue.cend();
    if (!qi::parse(begin, end, qi::double_ >> qi::eoi))
      return false;

    // The parse succeeded, hence the string is not empty. NaN and Inf are the
    // only accepted values whose first significant character is a letter.
    const std::size_t sign_len = (theValue.front() == '+' || theValue.front() == '-') ? 1 : 0;
    return std::isalpha(static_cast<unsigned char>(theValue[sign_len])) == 0;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the string looks like a number and normalize it
 */
// ----------------------------------------------------------------------

std::optional<std::string> TableFormatter::check_number(const std::string& theValue)
{
  try
  {
    if (!looks_number(theValue))
      return std::nullopt;

    // Check whether the string is already a valid JSON number and return it if so.
    // Note that the grammar is inlined into the parse call on purpose: naming the
    // parts would either construct a qi::rule on every call, which allocates, or
    // leave dangling references to the temporaries of the expression template.
    auto begin = theValue.cbegin();
    const auto end = theValue.cend();
    if (qi::parse(begin,
                  end,
                  -qi::lit('-')                                                // sign
                      >> (qi::lit('0') | (qi::char_('1', '9') >> *qi::digit))  // integer part
                      >> -(qi::lit('.') >> +qi::digit)                         // fraction part
                      >> -(qi::char_("eE") >> -qi::char_("+-") >> +qi::digit)  // exponent
                      >> qi::eoi))
      return theValue;

    // If we reach this point, the string is not a valid JSON number and needs normalization
    const std::size_t sign_len = (theValue.front() == '+' || theValue.front() == '-') ? 1 : 0;

    // The exponent needs no normalization, JSON accepts it as is
    const auto exp_pos = theValue.find_first_of("eE", sign_len);
    const auto mantissa = theValue.substr(
        sign_len, (exp_pos == std::string::npos ? theValue.size() : exp_pos) - sign_len);

    const auto dot_pos = mantissa.find('.');
    const auto integer_part = mantissa.substr(0, dot_pos);
    const auto fraction_part =
        (dot_pos == std::string::npos ? std::string() : mantissa.substr(dot_pos + 1));

    // JSON forbids leading zeroes, but requires at least one digit
    const auto first_digit = integer_part.find_first_not_of('0');

    std::string out;
    out.reserve(theValue.size() + 1);

    if (theValue.front() == '-')
      out += '-';
    out += (first_digit == std::string::npos ? std::string("0") : integer_part.substr(first_digit));

    // JSON forbids a trailing decimal point
    if (!fraction_part.empty())
    {
      out += '.';
      out += fraction_part;
    }

    if (exp_pos != std::string::npos)
      out += theValue.substr(exp_pos);

    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
