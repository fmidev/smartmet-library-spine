// ======================================================================
/*!
 * \brief Implementation of class Parameter
 */
// ======================================================================

#include "Parameter.h"
#include "Exception.h"
#include <boost/functional/hash.hpp>
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
    : itsName(theName), itsAlias(theName), itsType(theType), itsNumber(theNumber)
{
}

Parameter::Parameter(const std::string& theName,
                     const std::string& theAlias,
                     Type theType,
                     FmiParameterName theNumber)
    : itsName(theName), itsAlias(theAlias), itsType(theType), itsNumber(theNumber)
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a hash value for the parameter definition
 */
// ----------------------------------------------------------------------

std::size_t hash_value(const Parameter& theNumber)
{
  try
  {
    std::size_t seed = 0;
    boost::hash_combine(seed, theNumber.itsName);
    boost::hash_combine(seed, theNumber.itsAlias);
    boost::hash_combine(seed, theNumber.itsType);
    boost::hash_combine(seed, theNumber.itsNumber);
    return seed;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
