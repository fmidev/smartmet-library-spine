// ======================================================================
/*!
 * \brief Interface of class Table
 */
// ======================================================================

#pragma once

#include <boost/utility.hpp>

#include <list>
#include <set>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class Table : private boost::noncopyable
{
 public:
  Table();

  void set(std::size_t theColumn, std::size_t theRow, const std::string& theValue);
  const std::string& get(std::size_t theColumn, std::size_t theRow) const;
  std::string get(std::size_t theColumn, std::size_t theRow, const std::string& missing_text) const;

  bool empty() const;
  std::size_t mini() const { return itsMinI; }
  std::size_t maxi() const { return itsMaxI; }
  std::size_t minj() const { return itsMinJ; }
  std::size_t maxj() const { return itsMaxJ; }
  using Indexes = std::set<std::size_t>;
  Indexes columns() const;
  Indexes rows() const;

  void setMissingText(const std::string& missingText);
  const std::string& getMissingText() const;

 private:
  void build_array() const;

  // Elements are stored like this into a list
  struct element
  {
    std::size_t i;
    std::size_t j;
    std::string value;

    element(std::size_t theI, std::size_t theJ, const std::string& theValue)
        : i(theI), j(theJ), value(theValue)
    {
    }
  };

  // array limits
  std::size_t itsMinI;
  std::size_t itsMaxI;
  std::size_t itsMinJ;
  std::size_t itsMaxJ;

  std::string itsMissingText;

  // list of all elements
  std::list<element> itsList;

  // built when first get is done, and set will be disabled
  mutable std::vector<const std::string*> itsArray;

  // false if get has not been accessed yet
  mutable bool itsBuildingDone;

  const std::string itsEmptyValue;
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
