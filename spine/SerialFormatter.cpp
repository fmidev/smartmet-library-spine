// ======================================================================
/*!
 * \brief Implementation of class SerialFormatter
 */
// ======================================================================

#include "SerialFormatter.h"
#include "Convenience.h"
#include "HTTP.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <fmt/format.h>
#include <macgyver/Exception.h>
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
      throw Fmi::Exception(BCP, "Invalid attribute name '" + theName + "'");

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

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Format without attributes
 */
// ----------------------------------------------------------------------

std::string SerialFormatter::format_plain(const Table& theTable,
                                          const TableFormatter::Names& theNames,
                                          const HTTP::Request& theReq,
                                          Table::Indexes& theCols,
                                          Table::Indexes& theRows) const
{
  try
  {
    std::string out;
    out.reserve(TableFormatter::default_minimum_size);

    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    out += "a:";
    appendNumber(out, theRows.size());
    out += ":{";

    std::size_t row = 0;
    for (std::size_t j : theRows)
    {
      out += "i:";
      appendNumber(out, row++);
      out += ";a:";
      appendNumber(out, theCols.size());
      out += ":{";

      for (std::size_t i : theCols)
      {
        const std::string& name = theNames[i];
        const std::string& value = theTable.get(i, j);

        out += "s:";
        appendNumber(out, name.size());
        out += ":\"";
        out += name;
        out += "\";";

        out += "s:";
        appendNumber(out, value.empty() ? miss.size() : value.size());
        out += ":\"";
        if (value.empty())
          out += miss;
        else
          out += value;
        out += "\";";
      }
      out += "}";
    }

    out += "}";
    return out;
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

std::string SerialFormatter::format_attributes(const Table& theTable,
                                               const TableFormatter::Names& theNames,
                                               const HTTP::Request& theReq,
                                               Table::Indexes& theCols,
                                               Table::Indexes& theRows,
                                               std::list<std::string>& theAttributes) const
{
  try
  {
    std::string out;
    out.reserve(TableFormatter::default_minimum_size);

    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    if (theAttributes.size() == 0)
    {
      if (theRows.size() > 1)
      {
        out += "a:";
        appendNumber(out, theRows.size());
        out += ":{";
      }

      std::size_t row = 0;
      for (std::size_t j : theRows)
      {
        if (theRows.size() > 1)
        {
          out += "i:";
          appendNumber(out, row++);
          out += ';';
        }

        out += "a:";
        appendNumber(out, theCols.size());
        out += ":{";

        for (std::size_t i : theCols)
        {
          const std::string& name = theNames[i];
          out += "s:";
          appendNumber(out, name.size());
          out += ":\"";
          out += name;
          out += "\";";

          const std::string& value = theTable.get(i, j);
          out += "s:";
          appendNumber(out, value.empty() ? miss.size() : value.size());
          out += ":\"";
          if (value.empty())
            out += miss;
          else
            out += value;
          out += "\";";
        }
        out += "}";
      }
      if (theRows.size() > 1)
        out += "}";
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
        const std::string& value = theTable.get(nam, j);
        if (!value.empty())
          values.insert(value);
      }

      // Remove the attribute column temporarily

      theCols.erase(nam);
      std::remove(
          theAttributes.begin(), theAttributes.end(), attribute);  // NOLINT not using return value

      // Process unique attribute values one at a time

      out += "a:";
      appendNumber(out, values.size());
      out += ":{";

      for (const std::string& value : values)
      {
        Table::Indexes rows;
        for (std::size_t j : theRows)
        {
          if (theTable.get(nam, j) == value)
            rows.insert(j);
        }
        out += "s:";
        appendNumber(out, value.size());
        out += ":\"";
        out += value;
        out += "\";";

        out += format_attributes(theTable, theNames, theReq, theCols, rows, theAttributes);
      }
      out += "}";

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

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 */
// ----------------------------------------------------------------------

std::string SerialFormatter::format(const Table& theTable,
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
      return format_plain(theTable, theNames, theReq, cols, rows);

    return format_attributes(theTable, theNames, theReq, cols, rows, atts);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Avoid std::string allocations by appending via a buffer
void SerialFormatter::appendNumber(std::string& theOutput, std::size_t theNumber) const
{
  fmt::format_int f(theNumber);
  theOutput.append(f.data(), f.size());
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
