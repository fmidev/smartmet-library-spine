#pragma once

#include "LonLat.h"
#include "None.h"
#include "Table.h"
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <macgyver/TimeFormatter.h>
#include <macgyver/ValueFormatter.h>

namespace SmartMet
{
namespace Spine
{
class TableVisitor : public boost::static_visitor<>
{
 private:
  Table& itsTable;
  const Fmi::ValueFormatter& itsValueFormatter;
  std::vector<int> itsPrecisions;
  unsigned int itsCurrentColumn;
  unsigned int itsCurrentRow;
  boost::shared_ptr<Fmi::TimeFormatter> itsTimeFormatter;
  boost::optional<Fmi::TimeZonePtr> itsTimeZonePtr;
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
               boost::shared_ptr<Fmi::TimeFormatter> timeformatter,
               boost::optional<Fmi::TimeZonePtr> timezoneptr,
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
