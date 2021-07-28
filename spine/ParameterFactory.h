// ======================================================================
/*!
 * \brief Generate parameter descriptor from parameter name
 */
// ======================================================================

#pragma once

#include "Parameter.h"
#include "ParameterFunction.h"

#include <newbase/NFmiEnumConverter.h>
#include <string>

namespace SmartMet
{
namespace Spine
{
struct ParameterAndFunctions
{
  Parameter parameter;
  ParameterFunctions functions;

  ParameterAndFunctions(const Parameter& param, const ParameterFunctions& pfs)
      : parameter(param), functions(pfs)
  {
  }

  friend std::ostream& operator<<(std::ostream& out, const ParameterAndFunctions& paramfuncs);
};

std::ostream& operator<<(std::ostream& out, const ParameterAndFunctions& paramfuncs);

class ParameterFactory
{
 private:
  ParameterFactory();
  mutable NFmiEnumConverter converter;

  std::string extract_limits(const std::string& theString) const;
  std::string extract_function(const std::string& theString,
                               double& theLowerLimit,
                               double& theUpperLimit) const;
  FunctionId parse_function(const std::string& theFunction) const;
  std::string parse_parameter_functions(const std::string& theParameterRequest,
                                        std::string& theOriginalName,
                                        std::string& theParameterNameAlias,
                                        ParameterFunction& theInnerParameterFunction,
                                        ParameterFunction& theOuterParameterFunction) const;

 public:
  static const ParameterFactory& instance();
  Parameter parse(const std::string& name, bool ignoreBadParameter = false) const;
  ParameterAndFunctions parseNameAndFunctions(const std::string& name,
                                              bool ignoreBadParameter = false) const;

  // Newbase parameter conversion
  std::string name(int number) const;
  int number(const std::string& name) const;

};  // class ParameterFactory
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
