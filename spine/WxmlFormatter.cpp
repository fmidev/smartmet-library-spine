// ======================================================================
/*!
 * \brief Implementation of class WxmlFormatter
 */
// ======================================================================

#include "WxmlFormatter.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Table.h"
#include "TableFormatterOptions.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/spirit/include/qi.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if a string looks like a time stamp
 */
// ----------------------------------------------------------------------

bool looks_time(const std::string& theValue)
{
  bool utc;
  boost::posix_time::ptime t = Fmi::TimeParser::try_parse_iso(theValue, &utc);
  return !t.is_not_a_date_time();
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table in wxml 1.00
 *
 * Note: last 8 columns are always origintime, xmltime, weekday, timestring,
 * name, geoid, longitude and latitude
 */
// ----------------------------------------------------------------------

std::string WxmlFormatter::format_100(const Table& theTable,
                                      const TableFormatter::Names& theNames,
                                      const HTTP::Request& /* theReq */,
                                      const TableFormatterOptions& theConfig) const
{
  try
  {
    std::string out = R"(<?xml version="1.0" encoding="UTF-8" ?>)"
                      "\n";

    std::string schema = theConfig.wxmlSchema();

    boost::algorithm::replace_first(schema, "{version}", "1.00");

    if (schema.empty())
      out += "<pointweather>\n";
    else
    {
      out +=
          "<pointweather xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
          "xsi:schemaLocation=\"";
      out += schema;
      out += "\">\n";
    }

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    if (rows.empty())
    {
      out += "</pointweather>";
      return out;
    }

    std::vector<std::size_t> wcols;
    std::copy(cols.begin(), cols.end(), std::back_inserter(wcols));
    // TODO(mheiskan): Why not just do wcols = cols ???

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
    std::string origintime;
    for (std::size_t j : rows)
    {
      auto t = theTable.get(col_origintime, j);
      if (looks_time(t) && t > origintime)
        origintime = t;
    }
    out += "<meta>\n";
    out += "<updated>";
    out += origintime;
    out += "</updated>\n";
    out += "</meta>\n";

    std::string current_geoid = "ABCDEFGHIJKLMNOPQRSTUWXYZ";

    for (std::size_t j : rows)
    {
      if (theTable.get(col_geoid, j) != current_geoid)
      {
        if (j != 0)
          out += "</location>\n";

        current_geoid = theTable.get(col_geoid, j);

        out += "<location name=\"";
        out += theTable.get(col_name, j);
        out += "\" id=\"";
        out += theTable.get(col_geoid, j);
        out += "\" lon=\"";
        out += theTable.get(col_longitude, j);
        out += "\" lat=\"";
        out += theTable.get(col_latitude, j);
        out += "\">\n";
      }

      out += "<forecast time=\"";
      out += theTable.get(col_xmltime, j);
      out += "\" day=\"";
      out += theTable.get(col_weekday, j);
      out += "\">\n";

      for (std::size_t i : wcols)
      {
        const std::string& name = theNames[i];
        const auto& value = theTable.get(i, j);
        out += "<param name=\"";
        out += name;
        out += "\"";
        if (!value.empty())
        {
          if (looks_number(value))
            out += " value=\"";
          else
            out += " text=\"";
          out += value;
          out += "\"";
        }
        out += "/>\n";
      }
      out += "</forecast>\n";
    }
    out += "</location>\n";
    out += "</pointweather>\n";
    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

std::string WxmlFormatter::format_200(const Table& theTable,
                                      const TableFormatter::Names& theNames,
                                      const HTTP::Request& /* theReq */,
                                      const TableFormatterOptions& theConfig) const

{
  try
  {
    std::string out = R"(<?xml version="1.0" encoding="UTF-8" ?>)"
                      "\n";

    const auto& formatType = theConfig.formatType();
    std::string schema = theConfig.wxmlSchema();

    boost::algorithm::replace_first(schema, "{version}", "2.00");

    if (schema.empty())
      out += "<pointweather>\n";
    else
    {
      out +=
          "<pointweather xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
          "xsi:schemaLocation=\"";
      out += schema;
      out += "\">\n";
    }

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();
    if (rows.empty())
    {
      out += "</pointweather>";
      return out;
    }
    std::vector<std::size_t> wcols;
    std::copy(cols.begin(), cols.end(), std::back_inserter(wcols));
    // TODO(mheiskan): Why not just do wcols = cols???

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
    std::string origintime;
    for (std::size_t j : rows)
    {
      auto t = theTable.get(col_origintime, j);
      if (looks_time(t) && t > origintime)
        origintime = t;
    }
    out += "<meta>\n";
    out += "<updated>";
    out += origintime;
    out += "</updated>\n";
    out += "</meta>\n";
    std::string current_geoid = "ABCDEFGHIJKLMNOPQRSTUWXYZ";
    for (std::size_t j : rows)
    {
      if (theTable.get(col_geoid, j) != current_geoid)
      {
        if (j != 0)
          out += "</location>\n";

        current_geoid = theTable.get(col_geoid, j);
        out += "<location name=\"";
        out += theTable.get(col_name, j);
        out += "\" id=\"";
        out += theTable.get(col_geoid, j);
        out += "\" lon=\"";
        out += theTable.get(col_longitude, j);
        out += "\" lat=\"";
        out += theTable.get(col_latitude, j);
        out += "\">\n";
      }
      if (formatType == "observation")
      {
        out += "<observation time=\"";
      }
      else
      {
        out += "<forecast time=\"";
      }

      out += theTable.get(col_xmltime, j);
      out += "\"";

      auto& tstring = theTable.get(col_timestring, j);
      if (tstring != theTable.getMissingText() && !tstring.empty())
      {
        out += " timestring=\"";
        out += tstring;
        out += "\"";
      }

      out += ">\n";

      for (std::size_t i : wcols)
      {
        const std::string& name = theNames[i];
        const auto& value = theTable.get(i, j);
        out += "<param name=\"";
        out += name;
        out += "\"";
        if (!value.empty())
        {
          if (looks_number(value))
            out += " value=\"";
          else
            out += " text=\"";
          out += value;
          out += "\"";
        }
        out += "/>\n";
      }
      if (formatType == "observation")
      {
        out += "</observation>\n";
      }
      else
      {
        out += "</forecast>\n";
      }
    }
    out += "</location>\n";
    out += "</pointweather>\n";
    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 *
 * Note: last 4 columns are always xmltime, weekday, name and geoid
 */
// ----------------------------------------------------------------------

std::string WxmlFormatter::format(const Table& theTable,
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
      return format_100(theTable, theNames, theReq, theConfig);
    if (version == "2.00")
      return format_200(theTable, theNames, theReq, theConfig);

    throw Fmi::Exception(BCP, "Unsupported wxml version: " + version);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
