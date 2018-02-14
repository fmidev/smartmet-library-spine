// ======================================================================
/*!
 * \brief Implementation of class XmlFormatter
 */
// ======================================================================

#include "XmlFormatter.h"
#include "Convenience.h"
#include "Exception.h"
#include "Table.h"
#include "TableFormatterOptions.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/foreach.hpp>
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 */
// ----------------------------------------------------------------------

void XmlFormatter::format(std::ostream& theOutput,
                          const Table& theTable,
                          const TableFormatter::Names& theNames,
                          const HTTP::Request& theReq,
                          const TableFormatterOptions& theConfig) const
{
  try
  {
    theOutput << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" << std::endl;

    const std::string tag = theConfig.xmlTag();

    std::string style;
    auto givenstyle = theReq.getParameter("xmlstyle");

    if (!givenstyle)
      style = "attributes";
    else
      style = *givenstyle;

    if (style == "attributes")
      format_attributes(theOutput, theTable, theNames, tag, theReq);
    else if (style == "tags")
      format_tags(theOutput, theTable, theNames, tag, theReq);
    else if (style == "mixed")
    {
      if (theReq.getParameter("attributes"))
        format_mixed(theOutput, theTable, theNames, tag, theReq);
      else
        format_tags(theOutput, theTable, theNames, tag, theReq);
    }
    else
      throw Spine::Exception(BCP, "Unknown xmlstyle '" + style + "'");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using attributes style
 */
// ----------------------------------------------------------------------

void XmlFormatter::format_attributes(std::ostream& theOutput,
                                     const Table& theTable,
                                     const TableFormatter::Names& theNames,
                                     const std::string& theTag,
                                     const HTTP::Request& theReq) const
{
  try
  {
    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    theOutput << "<" << theTag << ">" << std::endl;

    BOOST_FOREACH (std::size_t j, rows)
    {
      theOutput << "<row";
      BOOST_FOREACH (std::size_t i, cols)
      {
        theOutput << ' ';
        const std::string& name = theNames[i];
        std::string value = theTable.get(i, j);
        boost::algorithm::replace_all(value, "\"", "&quot;");  // Escape possible quotes
        theOutput << name << "=\"" << (value.empty() ? miss : value) << '"';
      }
      theOutput << "/>" << std::endl;
    }
    theOutput << "</" << theTag << ">" << std::endl;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using tags style
 */
// ----------------------------------------------------------------------

void XmlFormatter::format_tags(std::ostream& theOutput,
                               const Table& theTable,
                               const TableFormatter::Names& theNames,
                               const std::string& theTag,
                               const HTTP::Request& theReq) const
{
  try
  {
    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    theOutput << "<" << theTag << ">" << std::endl;

    BOOST_FOREACH (std::size_t j, rows)
    {
      theOutput << "<row>" << std::endl;
      BOOST_FOREACH (std::size_t i, cols)
      {
        const std::string& name = theNames[i];
        std::string value = theTable.get(i, j);
        boost::algorithm::replace_all(value, "\"", "&quot;");  // Escape possible quotes
        theOutput << '<' << name << '>' << (value.empty() ? miss : value) << "</" << name << ">"
                  << std::endl;
      }
      theOutput << "</row>" << std::endl;
    }
    theOutput << "</" << theTag << ">" << std::endl;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using mixed style
 */
// ----------------------------------------------------------------------

void XmlFormatter::format_mixed(std::ostream& theOutput,
                                const Table& theTable,
                                const TableFormatter::Names& theNames,
                                const std::string& theTag,
                                const HTTP::Request& theReq) const
{
  try
  {
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
      throw Spine::Exception(BCP, "attribute list is required in mixed style formatting");
    else
      attribstring = *attr;

    std::set<std::string> atts = parse_xml_attributes(attribstring);

    theOutput << "<" << theTag << ">" << std::endl;

    BOOST_FOREACH (std::size_t j, rows)
    {
      theOutput << "<row";
      BOOST_FOREACH (std::size_t i, cols)
      {
        const std::string& name = theNames[i];
        if (atts.find(name) != atts.end())
        {
          std::string value = theTable.get(i, j);
          boost::algorithm::replace_all(value, "\"", "&quot;");  // Escape possible quotes
          theOutput << ' ' << name << "=\"" << (value.empty() ? miss : value) << '"';
        }
      }
      theOutput << ">" << std::endl;

      BOOST_FOREACH (std::size_t i, cols)
      {
        const std::string& name = theNames[i];
        if (atts.find(name) == atts.end())
        {
          std::string value = theTable.get(i, j);
          boost::algorithm::replace_all(value, "\"", "&quot;");  // Escape possible quotes
          theOutput << '<' << name << '>' << (value.empty() ? miss : value) << "</" << name << ">"
                    << std::endl;
        }
      }
      theOutput << "</row>" << std::endl;
    }
    theOutput << "</" << theTag << ">" << std::endl;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
