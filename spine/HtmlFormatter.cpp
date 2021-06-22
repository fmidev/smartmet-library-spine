// ======================================================================
/*!
 * \brief Implementation of class HtmlFormatter
 */
// ======================================================================

#include "HtmlFormatter.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Table.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <macgyver/Exception.h>
#include <iostream>
#include <set>
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

std::set<std::string> parse_html_attributes(const std::string& theStr)
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
 * \brief Format a 2D table using tags style
 */
// ----------------------------------------------------------------------

std::string HtmlFormatter::format(const Table& theTable,
                                  const TableFormatter::Names& theNames,
                                  const HTTP::Request& theReq,
                                  const TableFormatterOptions& /* theConfig */) const
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

    // Output headers

    std::string out = "<table><tr>";
    for (const auto& nam : cols)
    {
      const std::string& name = theNames[nam];
      out += "<th>";
      out += htmlescape(name);
      out += "</th>";
    }
    out += "</tr>";

    for (const auto j : rows)
    {
      out += "<tr>\n";
      for (const auto i : cols)
      {
        std::string value = htmlescape(theTable.get(i, j));
        out += "<td>";
        out += (value.empty() ? miss : value);
        out += "</td>";
      }
      out += "</tr>";
    }
    out += "</table>";
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
