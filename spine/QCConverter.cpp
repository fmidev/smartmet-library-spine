// ======================================================================
/*!
 * \brief Implementation of class QCConverter
 */
// ======================================================================

#include "QCConverter.h"
#include <macgyver/Exception.h>
#include <string>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

QCConverter::QCConverter()
{
  try
  {
    // Vector position 0 is not used
    m_keys.resize(10);

    // Conversion of old codes of 1, 2, 3 or 4 characters length

    // New code "0"
    m_keys.at(1)[0] = "0";  // "0"
    m_keys.at(2)[0] = "";   // "0x" not really defined
    m_keys.at(3)[0] = "";   // "0xx" not really defined
    m_keys.at(4)[0] = "";   // "0xxx" not really defined
    m_keys.at(1)[1] = "0";  // "1"
    m_keys.at(1)[6] = "0";  // "6"
    m_keys.at(1)[7] = "0";  // "7"
    m_keys.at(2)[6] = "0";  // "6x"
    m_keys.at(3)[6] = "0";  // "6xx"
    m_keys.at(4)[6] = "0";  // "6xxx"
    m_keys.at(2)[7] = "0";  // "7x"
    m_keys.at(3)[7] = "0";  // "7xx"
    m_keys.at(4)[7] = "0";  // "7xxx"

    // New code "1"
    m_keys.at(2)[1] = "1";  // "1x"
    m_keys.at(3)[1] = "1";  // "1xx"

    // New code "2"
    m_keys.at(4)[1] = "2";  // "1xxx"

    // New code "3"
    m_keys.at(1)[2] = "3";  // "2"
    m_keys.at(1)[4] = "3";  // "4"
    m_keys.at(1)[5] = "3";  // "5"
    m_keys.at(2)[2] = "3";  // "2x"
    m_keys.at(3)[2] = "3";  // "2xx"
    m_keys.at(2)[4] = "3";  // "4x"
    m_keys.at(3)[4] = "3";  // "4xx"
    m_keys.at(2)[5] = "3";  // "5x"
    m_keys.at(3)[5] = "3";  // "5xx"

    // New code "4"
    m_keys.at(4)[2] = "4";  // "2xxx"
    m_keys.at(4)[4] = "4";  // "4xxx"
    m_keys.at(4)[5] = "4";  // "5xxx"

    // New code "5"
    m_keys.at(1)[8] = "5";  // "8"
    m_keys.at(2)[8] = "5";  // "8x"
    m_keys.at(3)[8] = "5";  // "8xx"
    m_keys.at(4)[8] = "5";  // "8xxx"

    // New code "6"
    m_keys.at(1)[3] = "6";  // "3"
    m_keys.at(2)[3] = "6";  // "3x"
    m_keys.at(3)[3] = "6";  // "3xx"

    // New code "7"
    m_keys.at(4)[3] = "7";  // "3xxx"

    // New code "8"
    m_keys.at(1)[9] = "8";  // "9"
    m_keys.at(2)[9] = "8";  // "9x"
    m_keys.at(3)[9] = "8";  // "9xx"

    // New code "9"
    m_keys.at(4)[9] = "9";  // "9xxx"
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool QCConverter::convert(std::string& newCode, const std::string& oldCode)
{
  try
  {
    size_t oldCodeLength = oldCode.length();
    if ((oldCodeLength < 1) or (oldCodeLength > 4))
      return false;

    if (isdigit(oldCode[0]) == 0)
      return false;

    if (m_keys.at(oldCodeLength)[(oldCode[0] - '0')].length() == 0)
      return false;

    newCode = m_keys.at(oldCodeLength)[(oldCode[0] - '0')];

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool QCConverterOpen::convert(std::string& newCode, const std::string& oldCode)
{
  try
  {
    if (oldCode.length() == 1)
    {
      if (isdigit(oldCode[0]) == 0)
        return false;
      newCode = oldCode;
      return true;
    }

    return QCConverter::convert(newCode, oldCode);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
