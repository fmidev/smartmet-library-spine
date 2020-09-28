// ======================================================================
/*!
 * \brief Implementation of class Parameter
 */
// ======================================================================

#include "ParameterFunction.h"
#include <macgyver/Exception.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Generate a hash for the functions
 */
// ----------------------------------------------------------------------

std::string ParameterFunction::hash() const
{
  try
  {
    std::stringstream ss;

    switch (itsFunctionId)
    {
      case FunctionId::Mean:
        ss << "Mean";
        break;
      case FunctionId::Maximum:
        ss << "Max";
        break;
      case FunctionId::Minimum:
        ss << "Min";
        break;
      case FunctionId::Median:
        ss << "Med";
        break;
      case FunctionId::Sum:
        ss << "Sum";
        break;
      case FunctionId::Integ:
        ss << "Integ";
        break;
      case FunctionId::Change:
        ss << "Chg";
        break;
      case FunctionId::Trend:
        ss << "Trend";
        break;
      case FunctionId::Nearest:
        ss << "Nearest";
        break;
      case FunctionId::Interpolate:
        ss << "Interpolate";
        break;
      case FunctionId::StandardDeviation:
        ss << "Sdev";
        break;
      case FunctionId::Percentage:
        ss << "Pct[" << itsLowerLimit << "," << itsUpperLimit << "]";
        break;
      case FunctionId::Count:
        ss << "Cnt[" << itsLowerLimit << "," << itsUpperLimit << "]";
        break;
      case FunctionId::NullFunction:
        return "";
    }

    if (itsFunctionType == FunctionType::TimeFunction)
      ss << "_t";
    else if (itsFunctionType == FunctionType::AreaFunction)
      ss << "_a";

    ss << "(" << itsAggregationIntervalBehind << "," << itsAggregationIntervalAhead << ")";

    return ss.str();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Printable information on the function
 */
// ----------------------------------------------------------------------

std::string ParameterFunction::info() const
{
  try
  {
    std::stringstream ss;

    switch (itsFunctionId)
    {
      case FunctionId::Mean:
        ss << "Mean";
        break;
      case FunctionId::Maximum:
        ss << "Maximum";
        break;
      case FunctionId::Minimum:
        ss << "Minimum";
        break;
      case FunctionId::Median:
        ss << "Median";
        break;
      case FunctionId::Sum:
        ss << "Sum";
        break;
      case FunctionId::Integ:
        ss << "Integ";
        break;
      case FunctionId::StandardDeviation:
        ss << "StandardDeviation";
        break;
      case FunctionId::Percentage:
        ss << "Percentage[" << itsLowerLimit << "," << itsUpperLimit << "]";
        break;
      case FunctionId::Count:
        ss << "Count[" << itsLowerLimit << "," << itsUpperLimit << "]";
        break;
      case FunctionId::Change:
        ss << "Change";
        break;
      case FunctionId::Trend:
        ss << "Trend";
        break;
      case FunctionId::Nearest:
        ss << "Nearest";
        break;
      case FunctionId::Interpolate:
        ss << "Interpolate";
        break;
      case FunctionId::NullFunction:
        ss << "NullFunction";
        break;
    }

    ss << ":";

    if (itsFunctionType == FunctionType::TimeFunction)
      ss << "TimeFunction(itsAggregationInterval=-" << itsAggregationIntervalBehind << "+"
         << itsAggregationIntervalAhead << " minutes)";
    else if (itsFunctionType == FunctionType::AreaFunction)
      ss << "AreaFunction";
    else
      ss << "NullFunctionType";

    ss << " => " << (itsNaNFunction ? "nan-function" : "not nan-function");

    return ss.str();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::ostream& operator<<(std::ostream& out, const ParameterFunction& paramfunc)
{
  out << paramfunc.info() << "\n";
  return out;
}

std::ostream& operator<<(std::ostream& out, const ParameterFunctions& paramfuncs)
{
  out << "innerfunction =\t" << paramfuncs.innerFunction << "\n"
      << "outerfunction =\t" << paramfuncs.outerFunction << "\n";
  return out;
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
