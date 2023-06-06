// ======================================================================
/*!
 * \brief Implementation of class CsvFormatter
 */
// ======================================================================

#include "CsvFormatter.h"
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
 * \brief Encode a CSV string
 */
// ----------------------------------------------------------------------

std::string escape_csv(const std::string& s)
{
  std::string out = "\"";
  for (auto c : s)
  {
    if (c == '"')
      out += "\\\"";
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

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 */
// ----------------------------------------------------------------------

std::string CsvFormatter::format(const Table& theTable,
                                 const TableFormatter::Names& theNames,
                                 const HTTP::Request& theReq,
                                 const TableFormatterOptions& /* theConfig */) const
{
  try
  {
    std::string miss = "NaN";
    auto missing = theReq.getParameter("missingtext");
    if (!!missing)
      miss = *missing;

    Table::Indexes cols = theTable.columns();
    Table::Indexes rows = theTable.rows();

    std::string out;
    out.reserve(TableFormatter::default_minimum_size);

    // First the header row

    for (const auto& name : theNames)
    {
      if (!out.empty())
        out += ',';
      out += escape_csv(name);
    }
    out += "\n";

    // Then the data

    for (std::size_t j : rows)
    {
      std::size_t col = 0;
      for (std::size_t i : cols)
      {
        if (col++ > 0)
          out += ',';

        const auto& value = theTable.get(i, j);
        if (value.empty())
          out += escape_csv(miss);
        else if (looks_number(value))
          out += value;
        else
          out += escape_csv(value);
      }
      out += "\n";
    }

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
