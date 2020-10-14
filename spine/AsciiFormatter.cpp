// ======================================================================
/*!
 * \brief Implementation of class AsciiFormatter
 */
// ======================================================================

#include "AsciiFormatter.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Table.h"
#include <macgyver/Exception.h>
#include <iostream>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table
 *
 * Relevant options are:
 *
 * separator - character string separating the fields
 */
// ----------------------------------------------------------------------

std::string AsciiFormatter::format(const Table& theTable,
                                   const TableFormatter::Names& /* theNames */,
                                   const HTTP::Request& theReq,
                                   const TableFormatterOptions& /* theConfig */) const
{
  try
  {
    std::string out;

    std::string sep;
    auto separator = theReq.getParameter("separator");

    if (!separator)
      sep = " ";
    else
      sep = *separator;

    std::string miss;
    auto missing = theReq.getParameter("missingtext");

    if (!missing)
      miss = "nan";
    else
      miss = *missing;

    Table::Indexes cols = theTable.columns();
    Table::Indexes rows = theTable.rows();

    for (std::size_t j : rows)
    {
      bool ifirst = true;
      for (std::size_t i : cols)
      {
        if (!ifirst)
          out += sep;
        ifirst = false;
        const auto& value = theTable.get(i, j);

        if (value.empty())
          out += miss;
        else
          out += value;
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
