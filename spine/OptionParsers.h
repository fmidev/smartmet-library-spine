#pragma once

#include "HTTP.h"
#include "Parameter.h"
#include "ParameterFactory.h"
#include "ParameterFunction.h"
#include "TimeSeriesGeneratorOptions.h"

#include <vector>

namespace SmartMet
{
namespace Spine
{
namespace OptionParsers
{
using ParameterList = std::vector<Parameter>;
using ParameterFunctionList = std::vector<ParameterAndFunctions>;

class ParameterOptions
{
 public:
  const ParameterList& parameters() const { return itsParameters; }
  const ParameterFunctionList& parameterFunctions() const { return itsParameterFunctions; }
  bool empty() const { return itsParameters.empty(); }
  ParameterList::size_type size() const { return itsParameters.size(); }
  void add(const Parameter& theParam);
  void add(const Parameter& theParam, const ParameterFunctions& theParamFunctions);

 private:
  ParameterList itsParameters;
  ParameterFunctionList itsParameterFunctions;

};  // class ParameterOptions

ParameterOptions parseParameters(const HTTP::Request& theReq);
TimeSeriesGeneratorOptions parseTimes(const HTTP::Request& theReq);

}  // namespace OptionParsers
}  // namespace Spine
}  // namespace SmartMet
