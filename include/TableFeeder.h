#pragma once

#include <string>
#include <vector>
#include <list>

#include "Table.h"
#include "ValueFormatter.h"
#include "TimeSeries.h"
#include "TimeSeriesOutput.h"

namespace SmartMet
{
namespace Spine
{
namespace TimeSeries
{
// feed data to Table
// usage1: tablefeeder << TimeSeries
// usage2: tablefeeder << TimeSeriesGroup

class TableFeeder
{
 private:
  const ValueFormatter& itsValueFormatter;
  const std::vector<int>& itsPrecisions;
  TableVisitor itsTableVisitor;

 public:
  TableFeeder(Table& table,
              const ValueFormatter& valueformatter,
              const std::vector<int>& precisions,
              unsigned int currentcolumn = 0)
      : itsValueFormatter(valueformatter),
        itsPrecisions(precisions),
        itsTableVisitor(table, valueformatter, precisions, currentcolumn, currentcolumn)
  {
  }

  TableFeeder(Table& table,
              const ValueFormatter& valueformatter,
              const std::vector<int>& precisions,
              boost::shared_ptr<Fmi::TimeFormatter> timeformatter,
              boost::optional<boost::local_time::time_zone_ptr> timezoneptr,
              unsigned int currentcolumn = 0)
      : itsValueFormatter(valueformatter),
        itsPrecisions(precisions),
        itsTableVisitor(table,
                        valueformatter,
                        precisions,
                        timeformatter,
                        timezoneptr,
                        currentcolumn,
                        currentcolumn)
  {
  }

  unsigned int getCurrentRow() const { return itsTableVisitor.getCurrentRow(); }
  unsigned int getCurrentColumn() const { return itsTableVisitor.getCurrentColumn(); }
  void setCurrentRow(unsigned int currentRow) { itsTableVisitor.setCurrentRow(currentRow); }
  void setCurrentColumn(unsigned int currentColumn)
  {
    itsTableVisitor.setCurrentColumn(currentColumn);
  }

  const TableFeeder& operator<<(const TimeSeries& ts);
  const TableFeeder& operator<<(const TimeSeriesGroup& ts_group);
  const TableFeeder& operator<<(const TimeSeriesVector& ts_vector);
  const TableFeeder& operator<<(const std::vector<Value>& value_vector);

  // Set LonLat formatting
  TableFeeder& operator<<(LonLatFormat newformat);
};

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet
