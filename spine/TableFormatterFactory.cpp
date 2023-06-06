// ======================================================================
/*!
 * \brief Implementation of namespace TableFormatterFactory
 */
// ======================================================================

#include "TableFormatterFactory.h"
#include "AsciiFormatter.h"
#include "CsvFormatter.h"
#include "DebugFormatter.h"
#include "HtmlFormatter.h"
#include "JsonFormatter.h"
#include "PhpFormatter.h"
#include "SerialFormatter.h"
#include "WxmlFormatter.h"
#include "XmlFormatter.h"
#include <macgyver/Exception.h>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
namespace TableFormatterFactory
{
// ----------------------------------------------------------------------
/*!
 * \brief Create a formatter
 */
// ----------------------------------------------------------------------

TableFormatter* create(const std::string& theName)
{
  try
  {
    if (theName == "ascii")
      return new AsciiFormatter();
    if (theName == "csv")
      return new CsvFormatter();
    if (theName == "xml")
      return new XmlFormatter();
    if (theName == "wxml")
      return new WxmlFormatter();
    if (theName == "serial")
      return new SerialFormatter();
    if (theName == "json")
      return new JsonFormatter();
    if (theName == "php")
      return new PhpFormatter();
    if (theName == "html")
      return new HtmlFormatter();
    if (theName == "debug")
      return new DebugFormatter();

    throw Fmi::Exception(BCP, "Unknown data format request '" + theName + "'");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TableFormatterFactory
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
