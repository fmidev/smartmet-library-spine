// ======================================================================
/*!
 * \brief Implementation of class DebugFormatter
 */
// ======================================================================

#include "DebugFormatter.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Table.h"
#include <macgyver/Exception.h>
#include <iostream>
#include <set>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
std::string DebugFormatter::missing_value(const HTTP::Request& theReq) const
{
  std::string miss;
  auto missing = theReq.getParameter("missingtext");

  if (!missing)
    miss = "nan";
  else
    miss = *missing;

  return miss;
}

std::string DebugFormatter::make_header(const Table& theTable) const
{
  std::string out;
  out.reserve(TableFormatter::default_minimum_size);

  out +=
      "<!DOCTYPE html><html><head><title>Debug mode output</title>"
      "<style>"
      "table {border-collapse: collapse;}"
      "th, td {border-bottom: 1px solid black; padding: 3px 0.5em 3px 0.5em; "
      "text-align: center;}"
      "tr:nth-child(even) {background-color: #f2f2f2;}"
      "tr:hover {background-color: #e2e2e2;}"
      "</style>\n"
      "</head><body>\n";
  const std::optional<std::string> title;
  if (title)
    out += "<h1>" + htmlescape(*title) + "</h1>\n";
  return out;
}

std::string DebugFormatter::make_content(const Table& theTable,
                                         const HTTP::Request& theReq,
                                         bool escape) const
{
  const auto miss = missing_value(theReq);
  const Table::Indexes cols = theTable.columns();
  const Table::Indexes rows = theTable.rows();

  std::string out;
  for (const auto j : rows)
  {
    out += "<tr>\n";
    for (const auto i : cols)
    {
      out += "<td>";
      const auto& value = theTable.get(i, j);
      if (value.empty())
        out += miss;
      else if (!escape)
        out += value;
      else
        out += htmlescape(value);
      out += "</td>";
    }
    out += "</tr>";
  }
  out += "</table></body></html>";
  return out;
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using tags style
 */
// ----------------------------------------------------------------------

std::string DebugFormatter::format(const Table& theTable,
                                   const TableFormatter::Names& theNames,
                                   const HTTP::Request& theReq,
                                   const TableFormatterOptions& /* theConfig */) const
{
  try
  {
    auto out = make_header(theTable);

    // Append headers

    out += "<table><tr>";

    const auto& names = theTable.getNames(theNames, false);

    for (const auto& nam : theTable.columns())
    {
      const std::string& name = nam >= names.size() ? "" : names[nam];
      out += "<th>";
      out += htmlescape(name);
      out += "</th>";
    }
    out += "</tr>";

    bool escape = true;
    out += make_content(theTable, theReq, escape);

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
