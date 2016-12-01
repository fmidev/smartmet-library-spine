#pragma once

#include <string>
#include <sstream>
#include <vector>

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/foreach.hpp>
#include <boost/variant.hpp>

#include "Table.h"
#include "ValueFormatter.h"
#include <macgyver/TimeFormatter.h>
#include "TimeSeries.h"

namespace SmartMet
{
namespace Spine
{
namespace TimeSeries
{
// write content of Value to ostream (not formatted)
std::ostream& operator<<(std::ostream& os, const Value& val);
// write content of TimeSeries to ostream (not formatted)
std::ostream& operator<<(std::ostream& os, const TimeSeries& ts);
// write content of TimeSeriesGroup to ostream (not formatted)
std::ostream& operator<<(std::ostream& os, const TimeSeriesGroup& tsg);
// write content of TimeSeriesVector to ostream (not formatted)
std::ostream& operator<<(std::ostream& os, const TimeSeriesVector& tsv);

enum class LonLatFormat
{
  LATLON,
  LONLAT
};

// format Value and write to output stream
// usage: boost::apply_visitor(ostream_visitor, Value);
class OStreamVisitor : public boost::static_visitor<>
{
 private:
  std::ostream& itsOutstream;
  const ValueFormatter& itsValueFormatter;
  int itsPrecision;
  LonLatFormat itsLonLatFormat;

 public:
  OStreamVisitor(std::ostream& outs, const ValueFormatter& valueformatter, int precision)
      : itsOutstream(outs),
        itsValueFormatter(valueformatter),
        itsPrecision(precision),
        itsLonLatFormat(LonLatFormat::LONLAT)
  {
  }

  void operator()(const None& none) const;
  void operator()(const std::string& str) const;
  void operator()(double d) const;
  void operator()(int i) const;
  void operator()(const LonLat& lonlat) const;
  void operator()(const boost::local_time::local_date_time& ldt) const;
};

// format Value into string
// usage: boost::apply_visitor(ostream_visitor, Value);
class StringVisitor : public boost::static_visitor<std::string>
{
 private:
  const ValueFormatter& itsValueFormatter;
  int itsPrecision;
  LonLatFormat itsLonLatFormat;

 public:
  StringVisitor(const ValueFormatter& valueformatter, int precision)
      : itsValueFormatter(valueformatter),
        itsPrecision(precision),
        itsLonLatFormat(LonLatFormat::LONLAT)
  {
  }

  void setLonLatFormat(LonLatFormat newFormat) { itsLonLatFormat = newFormat; }
  void setPrecision(int newPrecision) { itsPrecision = newPrecision; }
  std::string operator()(const None& none) const;
  std::string operator()(const std::string& str) const;
  std::string operator()(double d) const;
  std::string operator()(int i) const;
  std::string operator()(const LonLat& lonlat) const;
  std::string operator()(const boost::local_time::local_date_time& ldt) const;
};

// format Value and write to Table
// usage: boost::apply_visitor(table_visitor,Value);

class TableVisitor : public boost::static_visitor<>
{
 private:
  Table& itsTable;
  const ValueFormatter& itsValueFormatter;
  std::vector<int> itsPrecisions;
  unsigned int itsCurrentColumn;
  unsigned int itsCurrentRow;
  boost::shared_ptr<Fmi::TimeFormatter> itsTimeFormatter;
  boost::optional<boost::local_time::time_zone_ptr> itsTimeZonePtr;
  LonLatFormat itsLonLatFormat;

 public:
  TableVisitor(Table& table,
               const ValueFormatter& valueformatter,
               const std::vector<int>& precisions,
               unsigned int currentcolumn,
               unsigned int currentrow)
      : itsTable(table),
        itsValueFormatter(valueformatter),
        itsPrecisions(precisions),
        itsCurrentColumn(currentcolumn),
        itsCurrentRow(currentrow),
        itsTimeFormatter(),
        itsTimeZonePtr(),
        itsLonLatFormat(LonLatFormat::LONLAT)
  {
  }

  TableVisitor(Table& table,
               const ValueFormatter& valueformatter,
               const std::vector<int>& precisions,
               boost::shared_ptr<Fmi::TimeFormatter> timeformatter,
               boost::optional<boost::local_time::time_zone_ptr> timezoneptr,
               unsigned int currentcolumn,
               unsigned int currentrow)
      : itsTable(table),
        itsValueFormatter(valueformatter),
        itsPrecisions(precisions),
        itsCurrentColumn(currentcolumn),
        itsCurrentRow(currentrow),
        itsTimeFormatter(timeformatter),
        itsTimeZonePtr(timezoneptr),
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
  void operator()(const boost::local_time::local_date_time& ldt);

  // Set LonLat - value formatting
  TableVisitor& operator<<(LonLatFormat newformat);
};

// write content of Value
TableVisitor& operator<<(TableVisitor& tf, const Value& val);

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet
