// ======================================================================
/*!
 * \brief Implementation of class ImageFormatter
 */
// ======================================================================

#include "ImageFormatter.h"
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
// ----------------------------------------------------------------------
/*!
 * \brief Format a 2D table using tags style
 */
// ----------------------------------------------------------------------

std::string ImageFormatter::format(const Table& theTable,
                                   const TableFormatter::Names& /* theNames */,
                                   const HTTP::Request& theReq,
                                   const TableFormatterOptions& /* theConfig */) const
{
  try
  {
    auto out = make_header();

    out += "<table>";

    bool escape = false;
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
