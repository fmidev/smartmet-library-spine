// ======================================================================
/*!
 * \brief Implementation of class AsciiFormatter
 */
// ======================================================================

#include "AsciiFormatter.h"
#include "Convenience.h"
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

void AsciiFormatter::format(std::ostream& theOutput,
                            const Table& theTable,
                            const TableFormatter::Names& /* theNames */,
                            const HTTP::Request& theReq,
                            const TableFormatterOptions& /* theConfig */) const
{
  try
  {
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
          theOutput << sep;
        ifirst = false;
        const auto& value = theTable.get(i, j);

        if (value.empty())
          theOutput << miss;
        else
          theOutput << value;
      }
      theOutput << std::endl;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
