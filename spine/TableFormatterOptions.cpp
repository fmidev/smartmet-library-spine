// ======================================================================
/*!
 * \brief Implementation of TableFormatterOptions
 */
// ======================================================================

#include "TableFormatterOptions.h"
#include "Exception.h"
#include <stdexcept>

static const char* default_wxml_version = "2.00";
static const char* default_wxml_schema =
    "http://services.weatherproof.fi/schemas/pointweather_{version}.xsd";
static const char* default_wxml_timestring = "%Y-%b-%dT%H:%M:%S";
static const char* default_xml_tag = "result";

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

TableFormatterOptions::TableFormatterOptions()
    : itsDefaultWxmlVersion(default_wxml_version),
      itsDefaultWxmlTimeString(default_wxml_timestring),
      itsWxmlSchema(default_wxml_schema),
      itsDefaultXmlTag(default_xml_tag)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

TableFormatterOptions::TableFormatterOptions(const libconfig::Config& theConfig)
    : itsDefaultWxmlVersion(default_wxml_version), itsDefaultWxmlTimeString(default_wxml_timestring)
{
  try
  {
    if (theConfig.exists("wxml"))
    {
      theConfig.lookupValue("wxml.timestring", itsDefaultWxmlTimeString);
      theConfig.lookupValue("wxml.version", itsDefaultWxmlVersion);
      itsWxmlSchema =
          "http://services.weatherproof.fi/schemas/pointweather_" + itsDefaultWxmlVersion + ".xsd";

      // Is there an override in the config?
      theConfig.lookupValue("wxml.schema", itsWxmlSchema);
      // Check if observation is wanted instead of forecast
      theConfig.lookupValue("wxml.formattype", itsFormatType);
    }

    theConfig.lookupValue("xml.tag", itsDefaultXmlTag);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
