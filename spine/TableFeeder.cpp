#include "TableFeeder.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Spine
{

const TableFeeder& TableFeeder::operator<<(const TS::TimeSeries& ts)
{
  try
  {
    if (ts.empty())
      return *this;

    for (auto& t : ts)
    {
      TS::Value val = t.value;
      boost::apply_visitor(itsTableVisitor, val);
    }

    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const TableFeeder& TableFeeder::operator<<(const TS::TimeSeriesGroup& ts_group)
{
  try
  {
    // no time series
    if (ts_group.size() == 0)
      return *this;

    // one time series
    if (ts_group.size() == 1)
      return (*this << ts_group[0].timeseries);

    // several time series
    size_t n_locations = ts_group.size();
    size_t n_timestamps = ts_group[0].timeseries.size();
    // iterate through timestamps
    for (size_t i = 0; i < n_timestamps; i++)
    {
      std::stringstream ss;
	  TS::OStreamVisitor ostream_visitor(
          ss, itsValueFormatter, itsPrecisions[itsTableVisitor.getCurrentColumn()]);
      ostream_visitor << itsLonLatFormat;

      // get values of the same timestep from all locations and concatenate them into one string
      ss << "[";
      // iterate through locations
      for (size_t k = 0; k < n_locations; k++)
      {
        // take time series from k:th location
        const TS::TimeSeries& timeseries = ts_group[k].timeseries;

        if (k > 0)
          ss << " ";

        // take value from i:th timestep
        TS::Value val = timeseries[i].value;
        // append value to the ostream
        boost::apply_visitor(ostream_visitor, val);
      }
      // if no data added (timestep not included)
      if (ss.str().size() == 1)
        continue;

      ss << "]";
      std::string str_value(ss.str());
      // remove spaces
      while (str_value[1] == ' ')
        str_value.erase(1, 1);
      while (str_value[str_value.size() - 2] == ' ')
        str_value.erase(str_value.size() - 2, 1);

      TS::Value dv = str_value;
      boost::apply_visitor(itsTableVisitor, dv);
    }

    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const TableFeeder& TableFeeder::operator<<(const TS::TimeSeriesVector& ts_vector)
{
  try
  {
    // no time series
    if (ts_vector.size() == 0)
      return *this;

    unsigned int startRow(itsTableVisitor.getCurrentRow());

    for (const auto& tvalue : ts_vector)
    {
      itsTableVisitor.setCurrentRow(startRow);

      TS::TimeSeries ts(tvalue);
      for (auto& t : ts)
      {
        boost::apply_visitor(itsTableVisitor, t.value);
      }
      itsTableVisitor.setCurrentColumn(itsTableVisitor.getCurrentColumn() + 1);
    }

    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const TableFeeder& TableFeeder::operator<<(const std::vector<TS::Value>& value_vector)
{
  try
  {
    // no values
    if (value_vector.size() == 0)
      return *this;

    for (auto& val : value_vector)
    {
      boost::apply_visitor(itsTableVisitor, val);
    }

    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TableFeeder& TableFeeder::operator<<(TS::LonLatFormat newformat)
{
  try
  {
    itsLonLatFormat = newformat;
    this->itsTableVisitor << newformat;
    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
