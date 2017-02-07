// ======================================================================
/*!
 * \brief Interface of class ParameterFunction
 */
// ======================================================================

#pragma once

#include <string>
#include <vector>
#include <limits>

namespace SmartMet
{
namespace Spine
{
class ParameterFactory;

enum class FunctionId
{
  Mean = 0x1,
  Maximum,
  Minimum,
  Median,
  Sum,
  Integ,
  StandardDeviation,
  Percentage,
  Count,
  Change,
  Trend,
  NullFunction
};

enum class FunctionType
{
  TimeFunction = 0x0,
  AreaFunction,
  NullFunctionType
};

struct ParameterFunction
{
  friend class ParameterFactory;

  ParameterFunction()
      : itsFunctionId(FunctionId::NullFunction),
        itsFunctionType(FunctionType::NullFunctionType),
        itsLowerLimit(std::numeric_limits<double>::min()),
        itsUpperLimit(std::numeric_limits<double>::max()),
        itsAggregationIntervalBehind(std::numeric_limits<unsigned int>::max()),
        itsAggregationIntervalAhead(std::numeric_limits<unsigned int>::max()),
        itsNaNFunction(false)
  {
  }
  ParameterFunction(FunctionId theFunctionId,
                    FunctionType theFunctionType,
                    double theLowerLimit,
                    double theUpperLimit)
      : itsFunctionId(theFunctionId),
        itsFunctionType(theFunctionType),
        itsLowerLimit(theLowerLimit),
        itsUpperLimit(theUpperLimit),
        itsAggregationIntervalBehind(std::numeric_limits<unsigned int>::max()),
        itsAggregationIntervalAhead(std::numeric_limits<unsigned int>::max()),
        itsNaNFunction(false)
  {
  }

  bool exists() const { return itsFunctionType != FunctionType::NullFunctionType; }
  std::string info() const;
  FunctionId id() const { return itsFunctionId; }
  FunctionType type() const { return itsFunctionType; }
  double lowerLimit() const { return itsLowerLimit; }
  double upperLimit() const { return itsUpperLimit; }
  unsigned int getAggregationIntervalBehind() const { return itsAggregationIntervalBehind; }
  unsigned int getAggregationIntervalAhead() const { return itsAggregationIntervalAhead; }
  void setAggregationIntervalBehind(unsigned int theIntervalBehind)
  {
    itsAggregationIntervalBehind = theIntervalBehind;
  }
  void setAggregationIntervalAhead(unsigned int theIntervalAhead)
  {
    itsAggregationIntervalAhead = theIntervalAhead;
  }
  bool isNanFunction() const { return itsNaNFunction; }
  std::string hash() const;

  friend std::ostream& operator<<(std::ostream& out, const ParameterFunction& paramfunc);

 private:
  FunctionId itsFunctionId;
  FunctionType itsFunctionType;
  double itsLowerLimit;
  double itsUpperLimit;
  unsigned int itsAggregationIntervalBehind;
  unsigned int itsAggregationIntervalAhead;
  bool itsNaNFunction;
};

struct ParameterFunctions
{
  ParameterFunctions() {}
  ParameterFunctions(const ParameterFunction& theInnerFunction,
                     const ParameterFunction& theOuterFunction)
      : innerFunction(theInnerFunction), outerFunction(theOuterFunction)
  {
  }

  ParameterFunction innerFunction;
  ParameterFunction outerFunction;

  friend std::ostream& operator<<(std::ostream& out, const ParameterFunctions& paramfunc);
};

std::ostream& operator<<(std::ostream& out, const ParameterFunction& paramfunc);
std::ostream& operator<<(std::ostream& out, const ParameterFunctions& paramfuncs);

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
