// ======================================================================
/*!
 * \brief Implementation of class Parameter
 */
// ======================================================================

#include "Parameter.h"
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <sstream>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Construct named parameter
 *
 * Note: The name may also be a number!
 */
// ----------------------------------------------------------------------

Parameter::Parameter(const std::string& theName, Type theType, FmiParameterName theNumber)
    : itsName(theName),
      itsOriginalName(theName),
      itsAlias(theName),
      itsType(theType),
      itsNumber(theNumber)
{
}

Parameter::Parameter(const std::string& theName,
                     std::string theAlias,
                     Type theType,
                     FmiParameterName theNumber)
    : itsName(theName),
      itsOriginalName(theName),
      itsAlias(std::move(theAlias)),
      itsType(theType),
      itsNumber(theNumber)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Return parameter type information
 */
// ----------------------------------------------------------------------

std::string Parameter::typestring() const
{
  try
  {
    switch (itsType)
    {
      case Type::Data:
        return "Data";
      case Type::DataDerived:
        return "DataDerived";
      case Type::DataIndependent:
        return "DataIndependent";
    }
    // NOT REACHED
    return "";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t Parameter::hashValue() const
{
  std::size_t seed = 0;
  Fmi::hash_combine(seed, Fmi::hash_value(itsName));
  Fmi::hash_combine(seed, Fmi::hash_value(itsAlias));
  Fmi::hash_combine(seed, Fmi::hash_value(static_cast<int>(itsType)));
  Fmi::hash_combine(seed, Fmi::hash_value(static_cast<int>(itsNumber)));
  return seed;
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a hash value for the parameter definition
 */
// ----------------------------------------------------------------------

std::size_t hash_value(const Parameter& theParam)
{
  return theParam.hashValue();
}

// ----------------------------------------------------------------------
/*!
 * \brief Print the parameter
 */
// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& out, const Parameter& param)
{
  out << "parameter =\t{ alias=" << param.itsAlias << " type=";
  switch (param.itsType)
  {
    case Parameter::Type::Data:
      out << "Data";
      break;
    case Parameter::Type::DataDerived:
      out << "DataDerived";
      break;
    case Parameter::Type::DataIndependent:
      out << "DataIndepentent";
      break;
  }
  out << " number=" << static_cast<int>(param.itsNumber) << "\n";
  return out;
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
