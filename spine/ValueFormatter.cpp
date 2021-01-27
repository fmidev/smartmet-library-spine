// ======================================================================
/*!
 * \brief Implementation of class ValueFormatter
 */
// ======================================================================

#include "ValueFormatter.h"
#include "Convenience.h"
#include <macgyver/Exception.h>
#include <boost/optional.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <cmath>
#include <stdexcept>

static int powers_of_ten[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};

namespace SmartMet
{
namespace Spine
{
// c++14 would not need this
ValueFormatterParam::ValueFormatterParam(const std::string& theMissingText,
                                         const std::string& theAdjustField,
                                         const std::string& theFloatField,
                                         int theWidth,
                                         char theFill,
                                         bool theShowPos,
                                         bool theUpperCase)
    : missingText(theMissingText),
      adjustField(theAdjustField),
      floatField(theFloatField),
      width(theWidth),
      fill(theFill),
      showPos(theShowPos),
      upperCase(theUpperCase)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Create libfmt format string from iostream style settings
 *
 * Relevant options:
 *
 * missingtext - the replacement for NaN
 * width - the width manipulator argument
 * fill - the fill manipulator argument
 * adjustfield - left|right|internal
 * showpos - true|false
 * uppercase - true|false
 * floatfield - fixed|scientific
 *
 * From http://fmtlib.net/latest/syntax.html:
 *
 * format_spec ::=  [[fill]align][sign]["#"]["0"][width]["." precision][type]
 * fill        ::=  <a character other than '{' or '}'>
 * align       ::=  "<" | ">" | "^"
 * sign        ::=  "+" | "-" | " "
 * width       ::=  integer | "{" arg_id "}"
 * precision   ::=  integer | "{" arg_id "}"
 * type        ::=  int_type | "a" | "A" | "c" | "e" | "E" | "f" | "F" | "g" | "G" | "p" | "s"
 * int_type    ::=  "b" | "B" | "d" | "n" | "o" | "x" | "X"
 */
// ----------------------------------------------------------------------

void ValueFormatter::buildFormat(const ValueFormatterParam& param)
{
  itsMissingText = param.missingText;
  itsFloatField = param.floatField;

  std::string fmt = "{:";

  if (param.width > 0)
  {
    fmt += param.fill;  // fill character

    if (param.adjustField == "left")
      fmt += '<';
    else if (param.adjustField == "right")
      fmt += '>';
    else if (param.adjustField == "internal")
      ; // deprecated in newer fmt in favour of sign aware zero padding
    else if (param.adjustField == "center")  // libfmt extension
      fmt += '^';
  }

  if (param.showPos)  // sign for positive numbers
    fmt += '+';

  if (param.fill == '0' || param.adjustField == "internal")  // sign aware zero padding
    fmt += '0';

  if (param.width > 0)
    fmt += fmt::sprintf("%d", param.width);

  boost::optional<char> ntype;

  if (param.floatField == "fixed")
  {
    if (param.upperCase)
      ntype = 'F';
    else
      ntype = 'f';
  }
  else if (param.floatField == "scientific")
  {
    if (param.upperCase)
      ntype = 'E';
    else
      ntype = 'e';
  }
  else if (param.floatField == "none")
  {
    if (param.upperCase)
      ntype = 'G';
    else
      ntype = 'g';
  }

  if (ntype)
  {
    itsFormat = fmt + *ntype + '}';
    itsPrecisionFormat = fmt + ".{}" + *ntype + '}';
  }
  else
  {
    itsFormat = fmt + '}';
    itsPrecisionFormat = fmt + ".{}" + '}';
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

ValueFormatter::ValueFormatter(const HTTP::Request& theReq)
{
  try
  {
    ValueFormatterParam p{optional_string(theReq.getParameter("missingtext"), "nan"),
                          optional_string(theReq.getParameter("adjustfield"), "right"),
                          optional_string(theReq.getParameter("floatfield"), "fixed"),
                          optional_int(theReq.getParameter("width"), -1),
                          optional_char(theReq.getParameter("fill"), ' '),
                          optional_bool(theReq.getParameter("showpos"), false),
                          optional_bool(theReq.getParameter("uppercase"), false)};

    buildFormat(p);

    // Override for WXML
    if (optional_string(theReq.getParameter("format"), "") == "WXML")
      p.missingText = "NaN";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

ValueFormatter::ValueFormatter(const ValueFormatterParam& param)
{
  buildFormat(param);
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert a float to a string
 *
 */
// ----------------------------------------------------------------------

std::string ValueFormatter::format(double theValue, int thePrecision) const
{
  try
  {
    if (std::isnan(theValue))
      return itsMissingText;

    if (thePrecision < 0)
      return fmt::format(itsFormat, theValue);

    auto value = theValue;

    // Fix rounding only in fixed (or none) mode and only for reasonable precision settings
    if ((itsFloatField == "fixed" || itsFloatField == "none") &&
        (thePrecision >= 1 && thePrecision <= 7))
    {
      // Fix rounding issues inherent in floating point arithmetic ->
      // https://www.dot.nd.gov/manuals/materials/testingmanual/rounding.pdf
      if (itsFloatField == "none")
      {
        value = round(value * powers_of_ten[thePrecision - 1]) /
                static_cast<double>(powers_of_ten[thePrecision - 1]);
      }
      else
      {
        value = round(value * powers_of_ten[thePrecision]) /
                static_cast<double>(powers_of_ten[thePrecision]);
      }
    }

    return fmt::format(itsPrecisionFormat, value, thePrecision);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
