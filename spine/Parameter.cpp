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
                     const std::string& theAlias,
                     Type theType,
                     FmiParameterName theNumber)
    : itsName(theName),
      itsOriginalName(theName),
      itsAlias(theAlias),
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
      case Type::Landscaped:
        return "Landscaped";
    }
    // NOT REACHED
    return "";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a hash value for the parameter definition
 */
// ----------------------------------------------------------------------

std::size_t hash_value(const Parameter& theParam)
{
  try
  {
    std::size_t seed = 0;
    Fmi::hash_combine(seed, Fmi::hash_value(theParam.itsName));
    Fmi::hash_combine(seed, Fmi::hash_value(theParam.itsAlias));
    Fmi::hash_combine(seed, Fmi::hash_value(static_cast<int>(theParam.itsType)));
    Fmi::hash_combine(seed, Fmi::hash_value(static_cast<int>(theParam.itsNumber)));
    return seed;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
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
    case Parameter::Type::Landscaped:
      out << "Landscaped";
      break;
  }
  out << " number=" << static_cast<int>(param.itsNumber) << "\n";
  return out;
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
