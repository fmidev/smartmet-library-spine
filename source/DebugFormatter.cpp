// ======================================================================
/*!
 * \brief Implementation of class DebugFormatter
 */
// ======================================================================

#include "DebugFormatter.h"
#include "Exception.h"
#include "Table.h"
#include "Convenience.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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

    theOutput << "<html><head><title>Debug mode output</title>"
              << "<style>"
              << "table { border: 1px solid black; }"
              << "td { border: 1px solid black; text-align:right;}"
              << "</style>"
              << "</head><body>";

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================