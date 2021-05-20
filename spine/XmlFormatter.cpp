// ======================================================================
/*!
 * \brief Implementation of class XmlFormatter
 */
// ======================================================================

#include "XmlFormatter.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Table.h"
#include "TableFormatterOptions.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <macgyver/Exception.h>
#include <iostream>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Convert a comma separated string into a set of strings
 */
// ----------------------------------------------------------------------

std::set<std::string> parse_xml_attributes(const std::string& theStr)
{
  try
  {
    std::set<std::string> ret;
    boost::algorithm::split(ret, theStr, boost::algorithm::is_any_of(","));
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 */
// ----------------------------------------------------------------------

std::string XmlFormatter::format(const Table& theTable,
                                 const TableFormatter::Names& theNames,
                                 const HTTP::Request& theReq,
                                 const TableFormatterOptions& theConfig) const
{
  try
  {
    std::string out = R"(<?xml version="1.0" encoding="UTF-8" ?>)"
                      "\n";

    const auto& tag = theConfig.xmlTag();

    std::string style;
    auto givenstyle = theReq.getParameter("xmlstyle");

    if (!givenstyle)
      style = "attributes";
    else
      style = *givenstyle;

    if (style == "attributes")
      out += format_attributes(theTable, theNames, tag, theReq);
    else if (style == "tags")
      out += format_tags(theTable, theNames, tag, theReq);
    else if (style == "mixed")
    {
      if (theReq.getParameter("attributes"))
        out += format_mixed(theTable, theNames, tag, theReq);
      else
        out += format_tags(theTable, theNames, tag, theReq);
    }
    else
      throw Fmi::Exception(BCP, "Unknown xmlstyle '" + style + "'");

    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using attributes style
 */
// ----------------------------------------------------------------------

std::string XmlFormatter::format_attributes(const Table& theTable,
                                            const TableFormatter::Names& theNames,
                                            const std::string& theTag,
                                            const HTTP::Request& theReq) const
{
  try
  {
    std::string out;

    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    out += '<';
    out += theTag;
    out += ">\n";

    for (std::size_t j : rows)
    {
      out += "<row";
      for (std::size_t i : cols)
      {
        out += ' ';
        const std::string& name = theNames[i];
        std::string value = theTable.get(i, j);
        boost::algorithm::replace_all(value, "\"", "&quot;");  // Escape possible quotes
        out += name;
        out += "=\"";
        out += (value.empty() ? miss : value);
        out += '"';
      }
      out += "/>\n";
    }
    out += "</";
    out += theTag;
    out += ">\n";
    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using tags style
 */
// ----------------------------------------------------------------------

std::string XmlFormatter::format_tags(const Table& theTable,
                                      const TableFormatter::Names& theNames,
                                      const std::string& theTag,
                                      const HTTP::Request& theReq) const
{
  try
  {
    std::string out;

    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    out += '<';
    out += theTag;
    out += ">\n";

    for (std::size_t j : rows)
    {
      out += "<row>\n";
      for (std::size_t i : cols)
      {
        const std::string& name = theNames[i];
        std::string value = theTable.get(i, j);
        boost::algorithm::replace_all(value, "\"", "&quot;");  // Escape possible quotes
        out += '<';
        out += name;
        out += '>';
        out += (value.empty() ? miss : value);
        out += "</";
        out += name;
        out += ">\n";
      }
      out += "</row>\n";
    }
    out += "</";
    out += theTag;
    out += ">\n";
    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using mixed style
 */
// ----------------------------------------------------------------------

std::string XmlFormatter::format_mixed(const Table& theTable,
                                       const TableFormatter::Names& theNames,
                                       const std::string& theTag,
                                       const HTTP::Request& theReq) const
{
  try
  {
    std::string out;

    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    std::string attribstring;
    auto attr = theReq.getParameter("attributes");
    if (!attr)
      throw Fmi::Exception(BCP, "attribute list is required in mixed style formatting");

    attribstring = *attr;

    std::set<std::string> atts = parse_xml_attributes(attribstring);

    out += '<';
    out += theTag;
    out += ">\n";

    for (std::size_t j : rows)
    {
      out += "<row";
      for (std::size_t i : cols)
      {
        const std::string& name = theNames[i];
        if (atts.find(name) != atts.end())
        {
          std::string value = theTable.get(i, j);
          boost::algorithm::replace_all(value, "\"", "&quot;");  // Escape possible quotes
          out += ' ';
          out += name;
          out += "=\"";
          out += (value.empty() ? miss : value);
          out += '"';
        }
      }
      out += ">\n";

      for (std::size_t i : cols)
      {
        const std::string& name = theNames[i];
        if (atts.find(name) == atts.end())
        {
          std::string value = theTable.get(i, j);
          boost::algorithm::replace_all(value, "\"", "&quot;");  // Escape possible quotes
          out += '<';
          out += name;
          out += '>';
          out += (value.empty() ? miss : value);
          out += "</";
          out += name;
          out += ">\n";
        }
      }
      out += "</row>\n";
    }
    out += "</";
    out += theTag;
    out += ">\n";
    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
