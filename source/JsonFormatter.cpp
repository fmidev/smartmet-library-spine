// ======================================================================
/*!
 * \brief Implementation of class JsonFormatter
 */
// ======================================================================

#include "JsonFormatter.h"
#include "Table.h"
#include "Convenience.h"
#include "Exception.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/foreach.hpp>
#include <boost/spirit/include/qi.hpp>
#include <algorithm>
#include <iostream>
#include <list>
#include <set>

namespace SmartMet
{
namespace Spine
{
namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Test if string looks like a number
 */
// ----------------------------------------------------------------------

bool looks_number(const std::string& theValue)
{
  try
  {
    double result;
    std::string::const_iterator begin = theValue.begin(), end = theValue.end();
    if (boost::spirit::qi::parse(begin, end, boost::spirit::qi::double_, result))
    {
      if (begin == end)
        return true;
    }
    return false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

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
      throw SmartMet::Spine::Exception(BCP, "Invalid attribute name '" + theName + "'!");

    return nam;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format recursion
 */
// ----------------------------------------------------------------------

void format_recursively(std::ostream& theOutput,
                        const Table& theTable,
                        const TableFormatter::Names& theNames,
                        const HTTP::Request& theReq,
                        Table::Indexes& theCols,
                        Table::Indexes& theRows,
                        std::list<std::string>& theAttributes)
{
  try
  {
    const std::string miss = "null";
    if (theAttributes.empty())
    {
      // Format the json always as an array
      theOutput << '[';
      std::size_t row = 0;
      BOOST_FOREACH (std::size_t j, theRows)
      {
        if (row++ > 0)
          theOutput << ',';

        theOutput << "{";

        std::size_t col = 0;
        BOOST_FOREACH (std::size_t i, theCols)
        {
          if (col++ > 0)
            theOutput << ',';

          const std::string& name = theNames[i];
          theOutput << '"' << name << '"';

          theOutput << ":";

          std::string value = theTable.get(i, j);
          if (value.empty())
            theOutput << miss;
          else if (value == "nan" || value == "NaN")  // nan is not allowed in JSON
            theOutput << "null";
          else if (looks_number(value))
            theOutput << value;
          else
            theOutput << '"' << value << '"';
        }
        theOutput << "}";
      }

      theOutput << ']';
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

      theOutput << '{';

      int att = 0;
      BOOST_FOREACH (const std::string& v, values)
      {
        if (att++ > 0)
          theOutput << ',';
        Table::Indexes rows;
        BOOST_FOREACH (std::size_t j, theRows)
        {
          if (theTable.get(nam, j) == v)
            rows.insert(j);
        }
        theOutput << '"' << v << "\":";
        format_recursively(theOutput, theTable, theNames, theReq, theCols, rows, theAttributes);
      }
      theOutput << '}';

      // Restore the attribute column

      theCols.insert(nam);
      theAttributes.push_front(attribute);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace anonymous

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 */
// ----------------------------------------------------------------------

void JsonFormatter::format(std::ostream& theOutput,
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

    format_recursively(theOutput, theTable, theNames, theReq, cols, rows, atts);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================