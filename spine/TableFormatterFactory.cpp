// ======================================================================
/*!
 * \brief Implementation of namespace TableFormatterFactory
 */
// ======================================================================

#include "TableFormatterFactory.h"
#include "AsciiFormatter.h"
#include "DebugFormatter.h"
#include "Exception.h"
#include "HtmlFormatter.h"
#include "JsonFormatter.h"
#include "PhpFormatter.h"
#include "SerialFormatter.h"
#include "WxmlFormatter.h"
#include "XmlFormatter.h"
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
    else if (theName == "xml")
      return new XmlFormatter();
    else if (theName == "wxml")
      return new WxmlFormatter();
    else if (theName == "serial")
      return new SerialFormatter();
    else if (theName == "json")
      return new JsonFormatter();
    else if (theName == "php")
      return new PhpFormatter();
    else if (theName == "html")
      return new HtmlFormatter();
    else if (theName == "debug")
      return new DebugFormatter();
    else
      throw Spine::Exception(BCP, "Unknown data format request '" + theName + "'");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TableFormatterFactory
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
