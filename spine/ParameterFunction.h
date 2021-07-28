// ======================================================================
/*!
 * \brief Interface of class ParameterFunction
 */
// ======================================================================

#pragma once

#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class ParameterFactory;

// 7 days
static const int MAX_AGGREGATION_INTERVAL = (7 * 24 * 60);

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
  Nearest,
  Interpolate,
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
        itsLowerLimit(std::numeric_limits<double>::lowest()),
        itsUpperLimit(std::numeric_limits<double>::max()),
        itsAggregationIntervalBehind(MAX_AGGREGATION_INTERVAL),
        itsAggregationIntervalAhead(MAX_AGGREGATION_INTERVAL),
        itsNaNFunction(false)
  {
    if (itsFunctionId == FunctionId::Nearest || itsFunctionId == FunctionId::Interpolate)
      itsNaNFunction = true;
  }
  ParameterFunction(FunctionId theFunctionId,
                    FunctionType theFunctionType,
                    double theLowerLimit = std::numeric_limits<double>::lowest(),
                    double theUpperLimit = std::numeric_limits<double>::max())
      : itsFunctionId(theFunctionId),
        itsFunctionType(theFunctionType),
        itsLowerLimit(theLowerLimit),
        itsUpperLimit(theUpperLimit),
        itsAggregationIntervalBehind(MAX_AGGREGATION_INTERVAL),
        itsAggregationIntervalAhead(MAX_AGGREGATION_INTERVAL),
        itsNaNFunction(false)
  {
    if (itsFunctionId == FunctionId::Nearest || itsFunctionId == FunctionId::Interpolate)
      itsNaNFunction = true;
  }
  ParameterFunction(const ParameterFunction& pf) = default;
  ParameterFunction& operator=(const ParameterFunction& pf) = default;
  bool exists() const { return itsFunctionType != FunctionType::NullFunctionType; }
  std::string info() const;
  FunctionId id() const { return itsFunctionId; }
  FunctionType type() const { return itsFunctionType; }
  double lowerLimit() const { return itsLowerLimit; }
  double upperLimit() const { return itsUpperLimit; }
  bool lowerOrUpperLimitGiven() const
  {
    return itsLowerLimit != std::numeric_limits<double>::lowest() ||
           itsUpperLimit != std::numeric_limits<double>::max();
  }
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
  void setLimits(double theLowerLimit, double theUpperLimit)
  {
    itsLowerLimit = theLowerLimit;
    itsUpperLimit = theUpperLimit;
  }

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
  ParameterFunctions() = default;
  ParameterFunctions(const ParameterFunction& theInnerFunction,
                     const ParameterFunction& theOuterFunction)
      : innerFunction(theInnerFunction), outerFunction(theOuterFunction)
  {
  }
  ParameterFunctions(const ParameterFunctions& functions) = default;
  ParameterFunctions& operator=(const ParameterFunctions& other) = default;

  ParameterFunction innerFunction;
  ParameterFunction outerFunction;

  friend std::ostream& operator<<(std::ostream& out, const ParameterFunctions& paramfunc);
};

std::ostream& operator<<(std::ostream& out, const ParameterFunction& paramfunc);
std::ostream& operator<<(std::ostream& out, const ParameterFunctions& paramfuncs);

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
