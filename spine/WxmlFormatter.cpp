// ======================================================================
/*!
 * \brief Implementation of class WxmlFormatter
 */
// ======================================================================

#include "WxmlFormatter.h"
#include "Exception.h"
#include "Table.h"
#include "TableFormatterOptions.h"
#include "Convenience.h"
#include <macgyver/StringConversion.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/spirit/include/qi.hpp>
#include <iostream>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Test if string looks like a number or is -INF, INF or NaN.
 */
// ----------------------------------------------------------------------

bool looks_number(const std::string& theValue)
{
  try
  {
    // Allow correct case special values
    if (theValue == "-INF" || theValue == "INF" || theValue == "NaN")
    {
      return true;
    }

    double result;
    std::string::const_iterator begin = theValue.begin(), end = theValue.end();
    if (boost::spirit::qi::parse(begin, end, boost::spirit::qi::double_, result))
      if (begin == end)
        return boost::math::isfinite(result);  // just in case spirit accepts nan, Inf etc
    return false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace anonymous

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table in wxml 1.00
 *
 * Note: last 8 columns are always origintime, xmltime, weekday, timestring,
 * name, geoid, longitude and latitude
 */
// ----------------------------------------------------------------------

void WxmlFormatter::format_100(std::ostream& theOutput,
                               const Table& theTable,
                               const TableFormatter::Names& theNames,
                               const HTTP::Request& /* theReq */,
                               const TableFormatterOptions& theConfig) const
{
  try
  {
    theOutput << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" << std::endl;

    std::string schema = theConfig.wxmlSchema();

    boost::algorithm::replace_first(schema, "{version}", "1.00");

    if (schema.empty())
      theOutput << "<pointweather>" << std::endl;
    else
      theOutput << "<pointweather xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                   "xsi:schemaLocation=\""
                << schema << "\">" << std::endl;

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    if (rows.empty())
    {
      theOutput << "</pointweather>";
      return;
    }

    std::vector<std::size_t> wcols;
    std::copy(cols.begin(), cols.end(), std::back_inserter(wcols));
    // TODO: Why not just do wcols = cols ???

    std::size_t col_latitude = wcols.back();
    wcols.pop_back();
    std::size_t col_longitude = wcols.back();
    wcols.pop_back();
    std::size_t col_geoid = wcols.back();
    wcols.pop_back();
    std::size_t col_name = wcols.back();
    wcols.pop_back();
    wcols.pop_back();  // timestring unused
    std::size_t col_weekday = wcols.back();
    wcols.pop_back();
    std::size_t col_xmltime = wcols.back();
    wcols.pop_back();
    std::size_t col_origintime = wcols.back();
    wcols.pop_back();

    // Establish latest origintime
    std::string origintime = "";
    BOOST_FOREACH (std::size_t j, rows)
    {
      if (theTable.get(col_origintime, j) > origintime)
        origintime = theTable.get(col_origintime, j);
    }
    theOutput << "<meta>" << std::endl;
    theOutput << "<updated>" << origintime << "</updated>" << std::endl;
    theOutput << "</meta>" << std::endl;

    std::string current_geoid = "ABCDEFGHIJKLMNOPQRSTUWXYZ";

    BOOST_FOREACH (std::size_t j, rows)
    {
      if (theTable.get(col_geoid, j) != current_geoid)
      {
        if (j != 0)
          theOutput << "</location>" << std::endl;

        current_geoid = theTable.get(col_geoid, j);

        theOutput << "<location name=\"" << theTable.get(col_name, j) << "\" id=\""
                  << theTable.get(col_geoid, j) << "\" lon=\"" << theTable.get(col_longitude, j)
                  << "\" lat=\"" << theTable.get(col_latitude, j) << "\">" << std::endl;
      }

      theOutput << "<forecast time=\"" << theTable.get(col_xmltime, j) << "\" day=\""
                << theTable.get(col_weekday, j) << "\">" << std::endl;

      BOOST_FOREACH (std::size_t i, wcols)
      {
        const std::string& name = theNames[i];
        std::string value = theTable.get(i, j);
        theOutput << "<param name=\"" << name << "\"";
        if (!value.empty())
        {
          if (looks_number(value))
            theOutput << " value=\"";
          else
            theOutput << " text=\"";
          theOutput << value << "\"";
        }
        theOutput << "/>" << std::endl;
      }
      theOutput << "</forecast>" << std::endl;
    }
    theOutput << "</location>" << std::endl;
    theOutput << "</pointweather>" << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table in wxml 2.00
 *
 * Note: last 8 columns are always origintime, xmltime, weekday, timestring,
 * name, geoid, longitude and latitude
 */
// ----------------------------------------------------------------------

void WxmlFormatter::format_200(std::ostream& theOutput,
                               const Table& theTable,
                               const TableFormatter::Names& theNames,
                               const HTTP::Request& /* theReq */,
                               const TableFormatterOptions& theConfig) const

{
  try
  {
    std::string formatType = theConfig.formatType();
    theOutput << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" << std::endl;
    std::string schema = theConfig.wxmlSchema();

    boost::algorithm::replace_first(schema, "{version}", "2.00");

    if (schema.empty())
      theOutput << "<pointweather>" << std::endl;
    else
      theOutput << "<pointweather xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                   "xsi:schemaLocation=\""
                << schema << "\">" << std::endl;

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();
    if (rows.empty())
    {
      theOutput << "</pointweather>";
      return;
    }
    std::vector<std::size_t> wcols;
    std::copy(cols.begin(), cols.end(), std::back_inserter(wcols));
    // TODO: Why not just do wcols = cols???

    std::size_t col_latitude = wcols.back();
    wcols.pop_back();
    std::size_t col_longitude = wcols.back();
    wcols.pop_back();
    std::size_t col_geoid = wcols.back();
    wcols.pop_back();
    std::size_t col_name = wcols.back();
    wcols.pop_back();
    std::size_t col_timestring = wcols.back();
    wcols.pop_back();
    wcols.pop_back();  // weekday unused
    std::size_t col_xmltime = wcols.back();
    wcols.pop_back();
    std::size_t col_origintime = wcols.back();
    wcols.pop_back();
    // Establish latest origintime
    std::string origintime = "";
    BOOST_FOREACH (std::size_t j, rows)
    {
      if (theTable.get(col_origintime, j) > origintime)
        origintime = theTable.get(col_origintime, j);
    }
    theOutput << "<meta>" << std::endl;
    theOutput << "<updated>" << origintime << "</updated>" << std::endl;
    theOutput << "</meta>" << std::endl;
    std::string current_geoid = "ABCDEFGHIJKLMNOPQRSTUWXYZ";
    BOOST_FOREACH (std::size_t j, rows)
    {
      if (theTable.get(col_geoid, j) != current_geoid)
      {
        if (j != 0)
          theOutput << "</location>" << std::endl;

        current_geoid = theTable.get(col_geoid, j);
        theOutput << "<location name=\"" << theTable.get(col_name, j) << "\" id=\""
                  << theTable.get(col_geoid, j) << "\" lon=\"" << theTable.get(col_longitude, j)
                  << "\" lat=\"" << theTable.get(col_latitude, j) << "\">" << std::endl;
      }
      if (formatType == "observation")
      {
        theOutput << "<observation time=\"";
      }
      else
      {
        theOutput << "<forecast time=\"";
      }

      theOutput << theTable.get(col_xmltime, j) << "\"";

      auto& tstring = theTable.get(col_timestring, j);
      if (tstring != theTable.getMissingText() && !tstring.empty())
        theOutput << " timestring=\"" << tstring << "\"";

      theOutput << ">" << std::endl;

      BOOST_FOREACH (std::size_t i, wcols)
      {
        const std::string& name = theNames[i];
        std::string value = theTable.get(i, j);
        theOutput << "<param name=\"" << name << "\"";
        if (!value.empty())
        {
          if (looks_number(value))
            theOutput << " value=\"";
          else
            theOutput << " text=\"";
          theOutput << value << "\"";
        }
        theOutput << "/>" << std::endl;
      }
      if (formatType == "observation")
      {
        theOutput << "</observation>" << std::endl;
      }
      else
      {
        theOutput << "</forecast>" << std::endl;
      }
    }
    theOutput << "</location>" << std::endl;
    theOutput << "</pointweather>" << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 *
 * Note: last 4 columns are always xmltime, weekday, name and geoid
 */
// ----------------------------------------------------------------------

void WxmlFormatter::format(std::ostream& theOutput,
                           const Table& theTable,
                           const TableFormatter::Names& theNames,
                           const HTTP::Request& theReq,
                           const TableFormatterOptions& theConfig) const
{
  try
  {
    std::string version;
    auto givenversion = theReq.getParameter("version");

    if (!givenversion)
      version = theConfig.defaultWxmlVersion();
    else
      version = *givenversion;

    if (version == "1.00")
      format_100(theOutput, theTable, theNames, theReq, theConfig);
    else if (version == "2.00")
      format_200(theOutput, theTable, theNames, theReq, theConfig);
    else
      throw SmartMet::Spine::Exception(BCP, "Unsupported wxml version: " + version);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
