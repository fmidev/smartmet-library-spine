#pragma once

#include "LonLat.h"
#include "macgyver/LocalDateTime.h"
#include <macgyver/ValueFormatter.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Fmi
{
class TimeFormatter;
}  // namespace Fmi
namespace SmartMet::Spine
{
class Table;
struct None;
}  // namespace SmartMet::Spine

namespace SmartMet
{
namespace Spine
{
class TableVisitor
{
 private:
  Table& itsTable;
  const Fmi::ValueFormatter& itsValueFormatter;
  std::vector<int> itsPrecisions;
  unsigned int itsCurrentColumn;
  unsigned int itsCurrentRow;
  std::shared_ptr<Fmi::TimeFormatter> itsTimeFormatter;
  std::optional<Fmi::TimeZonePtr> itsTimeZonePtr;
  LonLatFormat itsLonLatFormat;

 public:
  TableVisitor(Table& table,
               const Fmi::ValueFormatter& valueformatter,
               std::vector<int> precisions,
               unsigned int currentcolumn,
               unsigned int currentrow)
      : itsTable(table),
        itsValueFormatter(valueformatter),
        itsPrecisions(std::move(precisions)),
        itsCurrentColumn(currentcolumn),
        itsCurrentRow(currentrow),
        itsLonLatFormat(LonLatFormat::LONLAT)
  {
  }

  TableVisitor(Table& table,
               const Fmi::ValueFormatter& valueformatter,
               std::vector<int> precisions,
               std::shared_ptr<Fmi::TimeFormatter> timeformatter,
               std::optional<Fmi::TimeZonePtr> timezoneptr,
               unsigned int currentcolumn,
               unsigned int currentrow)
      : itsTable(table),
        itsValueFormatter(valueformatter),
        itsPrecisions(std::move(precisions)),
        itsCurrentColumn(currentcolumn),
        itsCurrentRow(currentrow),
        itsTimeFormatter(std::move(timeformatter)),
        itsTimeZonePtr(std::move(timezoneptr)),
        itsLonLatFormat(LonLatFormat::LONLAT)
  {
  }

  unsigned int getCurrentRow() const { return itsCurrentRow; }
  unsigned int getCurrentColumn() const { return itsCurrentColumn; }
  void setCurrentRow(unsigned int currentRow) { itsCurrentRow = currentRow; }
  void setCurrentColumn(unsigned int currentColumn) { itsCurrentColumn = currentColumn; }
  void operator()(const None& none);
  void operator()(const std::string& str);
  void operator()(double d);
  void operator()(int i);
  void operator()(const LonLat& lonlat);
  void operator()(const Fmi::LocalDateTime& ldt);

  // Set LonLat - value formatting
  TableVisitor& operator<<(LonLatFormat newformat);
};

}  // namespace Spine
}  // namespace SmartMet
