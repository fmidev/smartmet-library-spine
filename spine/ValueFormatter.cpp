// ======================================================================
/*!
 * \brief Implementation of class ValueFormatter
 */
// ======================================================================

#include "ValueFormatter.h"
#include "Exception.h"
#include "Convenience.h"
#include <iomanip>
#include <cmath>
#include <stdexcept>

static int powers_of_ten[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Default constructor for ValueFormatterParam
 */
//-----------------------------------------------------------------------

ValueFormatterParam::ValueFormatterParam()
    : missingText("nan"),
      adjustField("right"),
      floatField("fixed"),
      width(-1),
      fill(' '),
      showPos(false),
      upperCase(false)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

ValueFormatter::ValueFormatter(const HTTP::Request& theReq)
    : itsStream(new std::ostringstream),
      itsMissingText(optional_string(theReq.getParameter("missingtext"), "nan")),
      itsWidth(optional_int(theReq.getParameter("width"), -1)),
      itsFill(optional_char(theReq.getParameter("fill"), ' ')),
      itsAdjustField(optional_string(theReq.getParameter("adjustfield"), "right")),
      itsShowPos(optional_bool(theReq.getParameter("showpos"), false)),
      itsUpperCase(optional_bool(theReq.getParameter("uppercase"), false)),
      itsFloatField(optional_string(theReq.getParameter("floatfield"), "fixed"))
{
  try
  {
    // Override for WXML
    if (optional_string(theReq.getParameter("format"), "") == "WXML")
      itsMissingText = "NaN";
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

ValueFormatter::ValueFormatter(const ValueFormatterParam& param)
    : itsStream(new std::ostringstream),
      itsMissingText(param.missingText),
      itsWidth(param.width),
      itsFill(param.fill),
      itsAdjustField(param.adjustField),
      itsShowPos(param.showPos),
      itsUpperCase(param.upperCase),
      itsFloatField(param.floatField)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert a float to a string
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
 */
// ----------------------------------------------------------------------

std::string ValueFormatter::format(double theValue, int thePrecision) const
{
  try
  {
    if (isnan(theValue))
      return itsMissingText;

    // shorthand variable to avoid constant dereferencing
    std::ostringstream& out = *itsStream;

    double value = theValue;

    out.str("");  // empty old contents

    if (itsWidth > 0)
      out << std::setw(itsWidth);

    if (thePrecision >= 0)
    {
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
      out << std::setprecision(thePrecision);
    }
    if (itsShowPos)
      out << std::showpos;
    if (itsUpperCase)
      out << std::uppercase;
    out << std::setfill(itsFill);

    if (itsAdjustField == "left")
      out << std::left;
    else if (itsAdjustField == "right")
      out << std::right;
    else if (itsAdjustField == "internal")
      out << std::internal;
    else
      throw SmartMet::Spine::Exception(BCP, "Unknown adjustfield '" + itsAdjustField + "'");

    if (itsFloatField == "fixed")
      out << std::fixed;
    else if (itsFloatField == "scientific")
      out << std::scientific;
    else if (itsFloatField == "none")
      ;
    else
      throw SmartMet::Spine::Exception(BCP, "Unknown floatfield '" + itsFloatField + "'");

    out << value;

    return out.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
