// ======================================================================
/*!
 * \brief Implementation of class ValueFormatter
 */
// ======================================================================

#include "ValueFormatter.h"
#include "Convenience.h"
#include <boost/optional.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <macgyver/Exception.h>
#include <cmath>
#include <stdexcept>

#include <double-conversion/double-conversion.h>

static int powers_of_ten[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};

namespace SmartMet
{
namespace Spine
{
namespace
{
double round(double theValue, int thePrecision, const std::string& theMode)
{
  // Fix rounding issues inherent in floating point arithmetic ->
  // https://www.dot.nd.gov/manuals/materials/testingmanual/rounding.pdf
  if (theMode == "none")
    return (std::round(theValue * powers_of_ten[thePrecision - 1]) /
            static_cast<double>(powers_of_ten[thePrecision - 1]));

  return (std::round(theValue * powers_of_ten[thePrecision]) /
          static_cast<double>(powers_of_ten[thePrecision]));
}
}  // namespace

// c++14 would not need this
ValueFormatterParam::ValueFormatterParam(const std::string& theMissingText,
                                         const std::string& theFloatField)
    : missingText(theMissingText), floatField(theFloatField)
{
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
                          optional_string(theReq.getParameter("floatfield"), "fixed")};

    // Override for WXML
    if (optional_string(theReq.getParameter("format"), "") == "WXML")
      p.missingText = "NaN";
    itsFormatterParam = p;
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

ValueFormatter::ValueFormatter(const ValueFormatterParam& param) : itsFormatterParam(param) {}

std::string ValueFormatter::format(double theValue, int thePrecision) const
{
  if (itsFormatterParam.floatField == "scientific")
    return format_double_conversion_scientific(theValue, thePrecision);

  return format_double_conversion_fixed(theValue, thePrecision);
}

std::string ValueFormatter::format_double_conversion_scientific(double theValue,
                                                                int thePrecision) const
{
  try
  {
    if (isnan(theValue))
      return itsFormatterParam.missingText;

    using namespace double_conversion;

    const int kBufferSize = 168;
    char buffer[kBufferSize];
    StringBuilder builder(buffer, kBufferSize);
    int flags = (DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN |
                 DoubleToStringConverter::UNIQUE_ZERO);

    DoubleToStringConverter dc(
        flags, "Infinity", itsFormatterParam.missingText.c_str(), 'e', 0, 0, 0, 7);

    if (!dc.ToExponential(theValue, thePrecision, &builder))
      return itsFormatterParam.missingText;

    auto pos = builder.position();  // must be called before next row!

    builder.Finalize();  // required to avoid asserts in debug mode

    if (thePrecision < 0 && pos > 1 && (buffer[pos - 1] == '0' && buffer[pos - 2] == 'e'))
    {
      pos -= 2;
      while (pos > 0 &&
             (buffer[pos - 1] == '0' || (buffer[pos - 1] == ',' || buffer[pos - 1] == '.')))
        --pos;
    }

    return std::string(buffer, pos);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string ValueFormatter::format_double_conversion_fixed(double theValue, int thePrecision) const
{
  try
  {
    if (isnan(theValue))
      return itsFormatterParam.missingText;

    bool negativeValue = (theValue < 0);
    if (thePrecision == 0)
    {
      double ipart;
      double fractional = modf(theValue, &ipart);
      // Halfway rounding
      if (std::abs(fractional) == 0.5)
      {
        if (static_cast<int>(ipart) % 2 == 0)
          theValue = (theValue < 0 ? std::ceil(theValue) : std::floor(theValue));
        else
          theValue = (theValue < 0 ? std::floor(theValue) : std::ceil(theValue));
      }

      // If negative value is rounded up to zero, return here
      if (theValue == 0 && negativeValue)
        return Fmi::to_string(theValue);
    }

    // If precision is -1, return here
    if (thePrecision < 0)
      return Fmi::to_string(theValue);

    using namespace double_conversion;

    const int kBufferSize = 168;
    char buffer[kBufferSize];
    StringBuilder builder(buffer, kBufferSize);
    int flags = DoubleToStringConverter::UNIQUE_ZERO;

    DoubleToStringConverter dc(
        flags, "Infinity", itsFormatterParam.missingText.c_str(), 'e', 0, 0, 0, 7);

    if (thePrecision >= 1 && thePrecision <= 7)
      theValue = round(theValue, thePrecision, itsFloatField);

    if (!dc.ToFixed(theValue, thePrecision, &builder))
      return itsFormatterParam.missingText;

    auto pos = builder.position();  // must be called before next row!

    builder.Finalize();  // required to avoid asserts in debug mode

    if (itsFormatterParam.floatField == "none")
    {
      while (pos > 0 &&
             (buffer[pos - 1] == '0' || (buffer[pos - 1] == ',' || buffer[pos - 1] == '.')))
        --pos;
    }

    return std::string(buffer, pos);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
