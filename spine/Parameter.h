// ======================================================================
/*!
 * \brief Interface of class Parameter
 */
// ======================================================================

#pragma once

#include <newbase/NFmiParameterName.h>
#include <array>
#include <limits>
#include <string>

namespace SmartMet
{
namespace Spine
{
class Parameter
{
 public:
  enum class Type
  {
    Data,             // directly in querydata (temperature)
    DataDerived,      // calculated (wind chill, heat index etc)
    DataIndependent,  // coordinates, times, astrometry, ...
    Landscaped        // topography, land/sea corrections
  };

 public:
  Parameter(const std::string& theName,
            Type theType,
            FmiParameterName theNumber = kFmiBadParameter);

  Parameter(const std::string& theName,
            const std::string& theAlias,
            Type theType,
            FmiParameterName theNumber = kFmiBadParameter);

  const std::string& name() const { return itsName; }
  const std::string& originalName() const { return itsOriginalName; }
  std::string alias() const { return itsAlias; }
  void setAlias(const std::string& name) { itsAlias = name; }
  void setOriginalName(const std::string& name) { itsOriginalName = name; }
  FmiParameterName number() const { return itsNumber; }
  Type type() const { return itsType; }
  std::string typestring() const;

  friend std::size_t hash_value(const Parameter& theParam);

 private:
  friend std::ostream& operator<<(std::ostream& out, const Parameter& param);

  Parameter();
  // name contains plain parameter name, for example t2m
  std::string itsName;
  // extended name may contain parameter name with possible
  // function name or alternatively alias given by user
  // for example mean_t(t2m) as tmean
  // Original name is case sensitive since grid-parameter names may include LUA-function names
  std::string itsOriginalName;

  std::string itsAlias;
  Type itsType;
  FmiParameterName itsNumber;

};  // class Parameter

std::ostream& operator<<(std::ostream& out, const Parameter& param);

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
