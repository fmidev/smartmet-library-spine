// ======================================================================
/*!
 * \brief Implementation of class PhpFormatter
 */
// ======================================================================

#include "PhpFormatter.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Table.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
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
 * \brief Format without attributes
 */
// ----------------------------------------------------------------------

std::string format_plain(const Table& theTable,
                         const TableFormatter::Names& theNames,
                         const HTTP::Request& theReq,
                         const Table::Indexes& theCols,
                         const Table::Indexes& theRows)
{
  try
  {
    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    std::string out;
    out.reserve(TableFormatter::default_minimum_size);
    out += "array(\n";

    bool jfirst = true;
    for (std::size_t j : theRows)
    {
      if (!jfirst)
        out += ",\n";
      jfirst = false;

      out += "array(\n";

      bool ifirst = true;
      for (std::size_t i : theCols)
      {
        if (!ifirst)
          out += ",\n";
        ifirst = false;

        const std::string& name = theNames[i];
        std::string value = theTable.get(i, j);
        out += '"';
        out += name;
        out += "\" => \"";
        out += (value.empty() ? miss : value);
        out += "\"";
      }
      out += "\n)";
    }

    out += ");\n";
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

std::string format_attributes(const Table& theTable,
                              const TableFormatter::Names& theNames,
                              const HTTP::Request& theReq,
                              Table::Indexes& theCols,
                              Table::Indexes& theRows,
                              std::list<std::string>& theAttributes)
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
        out += "array(";
      bool jfirst = true;

      for (std::size_t j : theRows)
      {
        if (!jfirst)
          out += "),\narray(\n";
        jfirst = false;

        bool ifirst = true;
        for (std::size_t i : theCols)
        {
          if (!ifirst)
            out += ",\n";
          ifirst = false;

          const std::string& name = theNames[i];
          std::string value = theTable.get(i, j);
          out += '"';
          out += name;
          out += "\" => \"";
          out += (value.empty() ? miss : value);
          out += "\"";
        }
      }
      if (theRows.size() > 1)
        out += ')';
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
      theAttributes.remove(attribute);

      // Process unique attribute values one at a time

      bool vfirst = true;

      out += "array(\n";
      for (const std::string& value : values)
      {
        if (!vfirst)
          out += ",\n";
        vfirst = false;

        Table::Indexes rows;
        for (std::size_t j : theRows)
        {
          if (theTable.get(nam, j) == value)
            rows.insert(j);
        }
        out += '"';
        out += value;
        out += "\" => ";
        if (theAttributes.empty())
          out += "array(\n";

        out += format_attributes(theTable, theNames, theReq, theCols, rows, theAttributes);

        if (theAttributes.empty())
          out += ')';
      }
      out += ')';

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

std::string PhpFormatter::format(const Table& theTable,
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

    if (atts.size() == 0)
      return format_plain(theTable, theNames, theReq, cols, rows);

    return format_attributes(theTable, theNames, theReq, cols, rows, atts) + ";\n";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
