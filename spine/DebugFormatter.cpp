// ======================================================================
/*!
 * \brief Implementation of class DebugFormatter
 */
// ======================================================================

#include "DebugFormatter.h"
#include "Convenience.h"
#include "Exception.h"
#include "Table.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
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

std::set<std::string> parse_debug_attributes(const std::string& theStr)
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
 * \brief Format a 2D table using tags style
 */
// ----------------------------------------------------------------------

void DebugFormatter::format(std::ostream& theOutput,
                            const Table& theTable,
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

    theOutput << "<!DOCTYPE html><html><head><title>Debug mode output</title>"
              << "<style>"
              << "table {border-collapse: collapse;}"
              << "th, td {border-bottom: 1px solid black; padding: 3px 0.5em 3px 0.5em; "
              << "text-align: center;}"
              << "tr:nth-child(even) {background-color: #f2f2f2;}"
              << "tr:hover {background-color: #e2e2e2;}"
              << "</style>" << std::endl
              << "</head><body>" << std::endl;

    // Output headers

    theOutput << "<table><tr>";
    for (Table::Indexes::const_iterator nam = cols.begin(); nam != cols.end(); ++nam)
    {
      const std::string& name = theNames[*nam];
      theOutput << "<th>" << name << "</th>";
    }
    theOutput << "</tr>";

    for (Table::Indexes::const_iterator j = rows.begin(); j != rows.end(); ++j)
    {
      theOutput << "<tr>" << std::endl;
      for (Table::Indexes::const_iterator i = cols.begin(); i != cols.end(); ++i)
      {
        std::string value = theTable.get(*i, *j);
        theOutput << "<td>" << (value.empty() ? miss : value) << "</td>";
      }
      theOutput << "</tr>";
    }
    theOutput << "</table></body></html>";
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
