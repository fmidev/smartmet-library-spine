// ======================================================================
/*!
 * \brief Implementation of class SerialFormatter
 */
// ======================================================================

#include "SerialFormatter.h"
#include "Convenience.h"
#include "Exception.h"
#include "Table.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>
#include <algorithm>
#include <iostream>
#include <list>

namespace SmartMet
{
namespace Spine
{
namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Find ordinal of the given attribute name
 */
// ----------------------------------------------------------------------

unsigned int find_name(const std::string& theName, const TableFormatter::Names& theNames)
{
  try
  {
    unsigned int nam = 0;
    for (; nam < theNames.size(); ++nam)
    {
      if (theNames[nam] == theName)
        break;
    }

    if (nam >= theNames.size())
      throw Spine::Exception(BCP, "Invalid attribute name '" + theName + "'");

    return nam;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert a comma separated string into a list of strings
 */
// ----------------------------------------------------------------------

std::list<std::string> parse_attributes(const std::string& theStr)
{
  try
  {
    std::list<std::string> ret;
    if (!theStr.empty())
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
 * \brief Format without attributes
 */
// ----------------------------------------------------------------------

void format_plain(std::ostream& theOutput,
                  const Table& theTable,
                  const TableFormatter::Names& theNames,
                  const HTTP::Request& theReq,
                  Table::Indexes& theCols,
                  Table::Indexes& theRows)
{
  try
  {
    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    theOutput << "a:" << theRows.size() << ":{";

    std::size_t row = 0;
    BOOST_FOREACH (std::size_t j, theRows)
    {
      theOutput << "i:" << row++ << ";a:" << theCols.size() << ":{";

      BOOST_FOREACH (std::size_t i, theCols)
      {
        const std::string& name = theNames[i];
        std::string value = theTable.get(i, j);

        theOutput << "s:" << name.size() << ":\"" << name << "\";";

        if (value.empty())
          theOutput << "s:" << miss.size() << ":\"" << miss << "\";";
        else
          theOutput << "s:" << value.size() << ":\"" << value << "\";";
      }
      theOutput << "}";
    }

    theOutput << "}";
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format recursion
 */
// ----------------------------------------------------------------------

void format_attributes(std::ostream& theOutput,
                       const Table& theTable,
                       const TableFormatter::Names& theNames,
                       const HTTP::Request& theReq,
                       Table::Indexes& theCols,
                       Table::Indexes& theRows,
                       std::list<std::string>& theAttributes)
{
  try
  {
    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    if (theAttributes.size() == 0)
    {
      if (theRows.size() > 1)
        theOutput << "a:" << theRows.size() << ":{";

      std::size_t row = 0;
      BOOST_FOREACH (std::size_t j, theRows)
      {
        if (theRows.size() > 1)
          theOutput << "i:" << row++ << ";";

        theOutput << "a:" << theCols.size() << ":{";

        BOOST_FOREACH (std::size_t i, theCols)
        {
          const std::string& name = theNames[i];
          theOutput << "s:" << name.size() << ":\"" << name << "\";";

          std::string value = theTable.get(i, j);
          if (value.empty())
            theOutput << "s:" << miss.size() << ":\"" << miss << "\";";
          else
            theOutput << "s:" << value.size() << ":\"" << value << "\";";
        }
        theOutput << "}";
      }
      if (theRows.size() > 1)
        theOutput << "}";
    }
    else
    {
      // Find the ordinal of the attribute
      std::string attribute = theAttributes.front();
      theAttributes.pop_front();
      std::size_t nam = find_name(attribute, theNames);

      // Collect unique values in the attribute column

      std::set<std::string> values;
      BOOST_FOREACH (std::size_t j, theRows)
      {
        std::string value = theTable.get(nam, j);
        if (!value.empty())
          values.insert(value);
      }

      // Remove the attribute column temporarily

      theCols.erase(nam);
      std::remove(theAttributes.begin(), theAttributes.end(), attribute);

      // Process unique attribute values one at a time

      theOutput << "a:" << values.size() << ":{";

      BOOST_FOREACH (const std::string& value, values)
      {
        Table::Indexes rows;
        BOOST_FOREACH (std::size_t j, theRows)
        {
          if (theTable.get(nam, j) == value)
            rows.insert(j);
        }
        theOutput << "s:" << value.size() << ":\"" << value << "\";";

        format_attributes(theOutput, theTable, theNames, theReq, theCols, rows, theAttributes);
      }
      theOutput << "}";

      // Restore the attribute column

      theCols.insert(nam);
      theAttributes.push_front(attribute);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 */
// ----------------------------------------------------------------------

void SerialFormatter::format(std::ostream& theOutput,
                             const Table& theTable,
                             const TableFormatter::Names& theNames,
                             const HTTP::Request& theReq,
                             const TableFormatterOptions&) const
{
  try
  {
    std::string givenatts;
    auto attribs = theReq.getParameter("attributes");
    if (!attribs)
      givenatts = "";
    else
      givenatts = *attribs;

    std::list<std::string> atts = parse_attributes(givenatts);

    Table::Indexes cols = theTable.columns();
    Table::Indexes rows = theTable.rows();

    if (atts.size() == 0)
    {
      format_plain(theOutput, theTable, theNames, theReq, cols, rows);
    }
    else
    {
      format_attributes(theOutput, theTable, theNames, theReq, cols, rows, atts);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
