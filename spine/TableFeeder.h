#pragma once

#include <list>
#include <string>
#include <vector>

#include "Table.h"
#include "TableVisitor.h"
#include <timeseries/TimeSeriesInclude.h>
#include "ValueFormatter.h"

namespace SmartMet
{
namespace Spine
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
  TS::LonLatFormat itsLonLatFormat;

 public:
  TableFeeder(Table& table,
              const ValueFormatter& valueformatter,
              const std::vector<int>& precisions,
              unsigned int currentcolumn = 0)
      : itsValueFormatter(valueformatter),
        itsPrecisions(precisions),
        itsTableVisitor(table, valueformatter, precisions, currentcolumn, currentcolumn),
        itsLonLatFormat(TS::LonLatFormat::LONLAT)

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

  const TableFeeder& operator<<(const TS::TimeSeries& ts);
  const TableFeeder& operator<<(const TS::TimeSeriesGroup& ts_group);
  const TableFeeder& operator<<(const TS::TimeSeriesVector& ts_vector);
  const TableFeeder& operator<<(const std::vector<TS::Value>& value_vector);

  // Set LonLat formatting
  TableFeeder& operator<<(TS::LonLatFormat newformat);
};

}  // namespace Spine
}  // namespace SmartMet
