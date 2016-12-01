// ======================================================================
/*!
 * \brief Implementation of class AsciiFormatter
 */
// ======================================================================

#include "AsciiFormatter.h"
#include "Exception.h"
#include "Table.h"
#include "Convenience.h"
#include <boost/foreach.hpp>
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

    BOOST_FOREACH (std::size_t j, rows)
    {
      bool ifirst = true;
      BOOST_FOREACH (std::size_t i, cols)
      {
        if (!ifirst)
          theOutput << sep;
        ifirst = false;
        std::string value = theTable.get(i, j);

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
