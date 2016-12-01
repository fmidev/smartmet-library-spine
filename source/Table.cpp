// ======================================================================
/*!
 * \brief Implementation of class Table
 */
// ======================================================================

#include "Table.h"
#include "Exception.h"
#include <macgyver/TypeName.h>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <sstream>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

Table::Table()
    : itsMinI(),
      itsMaxI(),
      itsMinJ(),
      itsMaxJ(),
      itsMissingText(),
      itsList(),
      itsArray(),
      itsBuildingDone(false),
      itsEmptyValue()
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the table is empty
 */
// ----------------------------------------------------------------------

bool Table::empty() const
{
  return itsList.empty();
}

// ----------------------------------------------------------------------
/*!
 * \brief Set array value
 */
// ----------------------------------------------------------------------

void Table::set(std::size_t theColumn, std::size_t theRow, const std::string& theValue)
{
  try
  {
    if (itsBuildingDone)
      throw SmartMet::Spine::Exception(BCP,
                                       "Cannot set new values in Table once get has been called");

    // Recalculate array bounds

    if (itsList.empty())
    {
      itsMinI = itsMaxI = theColumn;
      itsMinJ = itsMaxJ = theRow;
    }
    else
    {
      itsMinI = std::min(itsMinI, theColumn);
      itsMaxI = std::max(itsMaxI, theColumn);
      itsMinJ = std::min(itsMinJ, theRow);
      itsMaxJ = std::max(itsMaxJ, theRow);
    }

    // Save the element

    itsList.push_back(element(theColumn, theRow, theValue));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get array value
 */
// ----------------------------------------------------------------------

const std::string& Table::get(std::size_t theColumn, std::size_t theRow) const
{
  try
  {
    using boost::format;
    using boost::str;
    // Cannot extract values from empty data

    if (itsList.empty())
      throw SmartMet::Spine::Exception(BCP, "Table::get does not work for empty tables");

    // We expect user to use mini() etc in loops to make sure loops are OK

    if (not((theColumn >= itsMinI) and (theColumn <= itsMaxI)))
    {
      SmartMet::Spine::Exception exception(BCP, "Column index is out of the range!");
      exception.addParameter("Column index", std::to_string(theColumn));
      exception.addParameter("Range", std::to_string(itsMinI) + ".." + std::to_string(itsMaxI));
      throw exception;
    }

    if (not((theRow >= itsMinJ) and (theRow <= itsMaxJ)))
    {
      SmartMet::Spine::Exception exception(BCP, "Row index is out of the range!");
      exception.addParameter("Row index", std::to_string(theRow));
      exception.addParameter("Range", std::to_string(itsMinJ) + ".." + std::to_string(itsMaxJ));
      throw exception;
    }

    // First call to get? Build array from list and disable further set calls

    if (!itsBuildingDone)
      build_array();

    // Calculate position in itsArray

    const std::size_t i = theColumn - itsMinI;
    const std::size_t j = theRow - itsMinJ;
    const std::size_t w = itsMaxI - itsMinI + 1;
    const std::size_t pos = j * w + i;

    return *itsArray[pos];
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get array value (replace values for out of range column index and also empty strings
 *        with value of missing text)
 */
// ----------------------------------------------------------------------
std::string Table::get(std::size_t theColumn,
                       std::size_t theRow,
                       const std::string& missing_text) const
{
  try
  {
    if (not itsList.empty() and not((theColumn >= itsMinI) and (theColumn <= itsMaxI)))
    {
      return missing_text;
    }
    else
    {
      const std::string& value = get(theColumn, theRow);
      return value == "" ? missing_text : value;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Build the quick access array into strings
 */
// ----------------------------------------------------------------------

void Table::build_array() const
{
  try
  {
    if (itsBuildingDone)
      return;

    // Initialize all elements to NULL. The array is still
    // of size 0 at this point, so resize will initialize
    // everything.

    const std::size_t w = itsMaxI - itsMinI + 1;
    const std::size_t h = itsMaxJ - itsMinJ + 1;
    const std::size_t sz = w * h;

    itsArray.resize(sz, &itsEmptyValue);

    // Assign pointers to valid array elements, the
    // rest will still point to empty

    BOOST_FOREACH (const element& e, itsList)
    {
      const std::size_t pos = (e.j - itsMinJ) * w + (e.i - itsMinI);
      itsArray[pos] = &(e.value);
    }

    itsBuildingDone = true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return column indices
 */
// ----------------------------------------------------------------------

Table::Indexes Table::columns() const
{
  try
  {
    if (!itsBuildingDone)
      build_array();

    Indexes ret;
    if (itsList.empty())
      return ret;

    for (std::size_t i = itsMinI; i <= itsMaxI; i++)
      ret.insert(i);

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return row indices
 */
// ----------------------------------------------------------------------

Table::Indexes Table::rows() const
{
  try
  {
    if (!itsBuildingDone)
      build_array();

    Indexes ret;
    if (itsList.empty())
      return ret;

    for (std::size_t j = itsMinJ; j <= itsMaxJ; j++)
      ret.insert(j);

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the missingtext
 */
// ----------------------------------------------------------------------

const std::string& Table::getMissingText() const
{
  return itsMissingText;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the missingtext
 */
// ----------------------------------------------------------------------

void Table::setMissingText(const std::string& missingText)
{
  itsMissingText = missingText;
}

}  // namespace Spine
}  // namespace SmartMet
