// ======================================================================
/*!
 * \brief Implementation of class JsonFormatter
 */
// ======================================================================

#include "JsonFormatter.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Table.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/spirit/include/qi.hpp>
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <algorithm>
#include <iomanip>
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
 * \brief Encode a JSON string
 *
 * Ref: http://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c
 */
// ----------------------------------------------------------------------

std::string escape_json(const std::string& s)
{
  std::string out = "\"";
  for (auto c : s)
  {
    if (c == '"' || c == '\\' || ('\x00' <= c && c <= '\x1f'))
    {
      out += fmt::format("\\u{:04x}", static_cast<int>(c));
    }
    else
      out += c;
  }
  out += '"';
  return out;
}

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
    auto begin = theValue.cbegin();
    auto end = theValue.cend();
    if (boost::spirit::qi::parse(begin, end, boost::spirit::qi::double_, result))
    {
      if (begin == end)
        return true;
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
      throw Fmi::Exception(BCP, "Invalid attribute name '" + theName + "'!");

    return nam;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format recursion
 */
// ----------------------------------------------------------------------

std::string format_recursively(const Table& theTable,
                               const TableFormatter::Names& theNames,
                               const HTTP::Request& theReq,
                               Table::Indexes& theCols,
                               const Table::Indexes& theRows,
                               std::list<std::string>& theAttributes)
{
  try
  {
    std::string out;
    out.reserve(TableFormatter::default_minimum_size);

    const std::string miss = "null";
    if (theAttributes.empty())
    {
      // Format the json always as an array
      out += '[';
      std::size_t row = 0;
      for (std::size_t j : theRows)
      {
        if (row++ > 0)
          out += ',';

        out += "{";

        std::size_t col = 0;
        for (std::size_t i : theCols)
        {
          if (col++ > 0)
            out += ',';

          const std::string& name = theNames[i];
          out += '"';
          out += name;
          out += '"';

          out += ':';

          const auto& value = theTable.get(i, j);
          if (value.empty())
            out += miss;
          else if (value == "nan" || value == "NaN")  // nan is not allowed in JSON
            out += "null";
          else if (looks_number(value))
            out += value;
          else
            out += escape_json(value);
        }
        out += '}';
      }

      out += ']';
    }
    else
    {
      // Find the ordinal of the attribute
      std::string attribute = theAttributes.front();
      theAttributes.pop_front();
      std::size_t nam = find_name(attribute, theNames);

      // Collect unique values in the attribute column

      std::set<std::string> values;
      for (std::size_t j : theRows)
      {
        const auto& value = theTable.get(nam, j);
        if (!value.empty())
          values.insert(value);
      }

      // Remove the attribute column temporarily

      theCols.erase(nam);
      theAttributes.remove(attribute);

      // Process unique attribute values one at a time

      out += '{';

      int att = 0;
      for (const std::string& v : values)
      {
        if (att++ > 0)
          out += ',';
        Table::Indexes rows;
        for (std::size_t j : theRows)
        {
          if (theTable.get(nam, j) == v)
            rows.insert(j);
        }
        out += '"';
        out += v;
        out += "\":";
        out += format_recursively(theTable, theNames, theReq, theCols, rows, theAttributes);
      }
      out += '}';

      // Restore the attribute column

      theCols.insert(nam);
      theAttributes.push_front(attribute);
    }
    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 */
// ----------------------------------------------------------------------

std::string JsonFormatter::format(const Table& theTable,
                                  const TableFormatter::Names& theNames,
                                  const HTTP::Request& theReq,
                                  const TableFormatterOptions& /* theConfig */) const
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

    const auto& names = theTable.getNames(theNames, true);

    return format_recursively(theTable, names, theReq, cols, rows, atts);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
