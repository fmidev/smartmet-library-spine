// ======================================================================
/*!
 * \brief Interface of class Table
 */
// ======================================================================

#pragma once

#include <list>
#include <set>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
class Table
{
 public:
  Table() = default;
  Table(const Table& other) = delete;
  Table(Table&& other) = delete;
  Table& operator=(const Table& other) = delete;
  Table& operator=(Table&& other) = delete;

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

  void setPaging(std::size_t startRow, std::size_t maxResults);

 private:
  void build_array() const;

  // Elements are stored like this into a list
  struct element
  {
    std::size_t i = 0;
    std::size_t j = 0;
    std::string value;

    element(std::size_t theI, std::size_t theJ, std::string theValue)
        : i(theI), j(theJ), value(std::move(theValue))
    {
    }
  };

  // array limits
  std::size_t itsMinI = 0;
  std::size_t itsMaxI = 0;
  std::size_t itsMinJ = 0;
  std::size_t itsMaxJ = 0;

  std::string itsMissingText;

  // list of all elements
  std::list<element> itsList;

  // built when first get is done, and set will be disabled
  mutable std::vector<const std::string*> itsArray;

  // false if get has not been accessed yet
  mutable bool itsBuildingDone = false;

  const std::string itsEmptyValue;

  // paging
  std::size_t itsStartRow = 0;    // 0 == first
  std::size_t itsMaxResults = 0;  // 0 == all
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
