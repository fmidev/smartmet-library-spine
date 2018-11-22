#include "TimeSeriesAggregator.h"
#include "Convenience.h"
#include "Exception.h"
#include "TimeSeries.h"
#include "TimeSeriesOutput.h"
#include <newbase/NFmiGlobals.h>
#include <cmath>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::local_time;

// #define MYDEBUG 1

namespace SmartMet
{
namespace Spine
{
namespace TimeSeries
{
class StatCalculator
{
 private:
  // doubles into itsDataVector, other types into itsTimeSeries
  // Note! NaN values are always put into itsTimeSeries, because they are strings,
  // so there can be data in both vectors
  Fmi::Stat::DataVector itsDataVector;
  TimeSeries itsTimeSeries;

  double getDoubleStatValue(const ParameterFunction& func, bool useWeights) const;
  std::string getStringStatValue(const ParameterFunction& func) const;
  boost::local_time::local_date_time getLocalDateTimeStatValue(const ParameterFunction& func) const;
  LonLat getLonLatStatValue(const ParameterFunction& func) const;

 public:
  void operator()(const TimedValue& tv);
  Value getStatValue(const ParameterFunction& func, bool useWeights) const;
};

double StatCalculator::getDoubleStatValue(const ParameterFunction& func, bool useWeights) const
{
  try
  {
    Fmi::Stat::Stat stat(itsDataVector, kFloatMissing);
    stat.useWeights(useWeights);

    switch (func.id())
    {
      case FunctionId::Mean:
        return stat.mean();
      case FunctionId::Maximum:
        return stat.max();
      case FunctionId::Minimum:
        return stat.min();
      case FunctionId::Median:
        return stat.median();
      case FunctionId::Sum:
        stat.useWeights(false);
        return stat.sum();
      case FunctionId::Integ:
        return stat.integ();
      case FunctionId::StandardDeviation:
        return stat.stddev();
      case FunctionId::Percentage:
        return stat.percentage(func.lowerLimit(), func.upperLimit());
      case FunctionId::Count:
        return stat.count(func.lowerLimit(), func.upperLimit());
      case FunctionId::Change:
        return stat.change();
      case FunctionId::Trend:
        return stat.trend();
      case FunctionId::NullFunction:
        return kFloatMissing;
#ifndef UNREACHABLE
      default:
        return kFloatMissing;
#endif
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string StatCalculator::getStringStatValue(const ParameterFunction& func) const
{
  try
  {
    FunctionId fid(func.id());

    if (fid == FunctionId::Maximum)
      return boost::get<std::string>(itsTimeSeries[itsTimeSeries.size() - 1].value);
    else if (fid == FunctionId::Minimum)
      return boost::get<std::string>(itsTimeSeries[0].value);
    else if (fid == FunctionId::Sum || fid == FunctionId::Integ)
    {
      std::stringstream ss;
      std::cout << "HERE2\n";

      ss << "[";
      for (const TimedValue& tv : itsTimeSeries)
      {
        if (ss.str().size() > 1)
          ss << " ";
        ss << boost::get<std::string>(tv.value);
      }
      ss << "]";
      std::cout << "HERE2.1: " << ss.str() << std::endl;
      return ss.str();
    }
    else if (fid == FunctionId::Median)
      return boost::get<std::string>(itsTimeSeries[itsTimeSeries.size() / 2].value);
    else if (fid == FunctionId::Count)
    {
      // Fmi::Stat::Count functions can not be applid to strings, so
      // first add timesteps into data vector with double value 1.0,
      // then call Fmi::Stat::count-function
      Fmi::Stat::DataVector dataVector;
      for (auto item : itsTimeSeries)
        dataVector.push_back(Fmi::Stat::DataItem(item.time.utc_time(), 1.0));
      Fmi::Stat::Stat stat(dataVector, kFloatMissing);
      return Fmi::to_string(stat.count(func.lowerLimit(), func.upperLimit()));
    }
    else
    {
      std::stringstream ss;
      ss << "Function " << func.hash() << " can not be applied for a string!";
      throw Spine::Exception(BCP, ss.str().c_str());
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::local_time::local_date_time StatCalculator::getLocalDateTimeStatValue(
    const ParameterFunction& func) const
{
  try
  {
    FunctionId fid(func.id());

    if (fid == FunctionId::Maximum)
      return boost::get<boost::local_time::local_date_time>(
          itsTimeSeries[itsTimeSeries.size() - 1].value);
    else if (fid == FunctionId::Minimum)
      return boost::get<boost::local_time::local_date_time>(itsTimeSeries[0].value);
    else if (fid == FunctionId::Median)
      return boost::get<boost::local_time::local_date_time>(
          itsTimeSeries[itsTimeSeries.size() / 2].value);
    else
    {
      std::stringstream ss;
      ss << "Function " << func.hash() << " can not be applied for a date!";
      throw Spine::Exception(BCP, ss.str().c_str());
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

LonLat StatCalculator::getLonLatStatValue(const ParameterFunction& func) const
{
  try
  {
    std::vector<double> lon_vector;
    std::vector<double> lat_vector;

    for (const TimedValue& tv : itsTimeSeries)
    {
      Value value(tv.value);
      lon_vector.push_back((boost::get<LonLat>(value)).lon);
      lat_vector.push_back((boost::get<LonLat>(value)).lat);
    }

    FunctionId fid(func.id());

    if (fid == FunctionId::Maximum)
    {
      std::vector<double>::iterator iter;
      iter = std::max_element(lon_vector.begin(), lon_vector.end());
      double lon_max(*iter);
      iter = std::max_element(lat_vector.begin(), lat_vector.end());
      double lat_max(*iter);
      return LonLat(lon_max, lat_max);
    }
    else if (fid == FunctionId::Minimum)
    {
      std::vector<double>::iterator iter;
      iter = std::min_element(lon_vector.begin(), lon_vector.end());
      double lon_min(*iter);
      iter = std::min_element(lat_vector.begin(), lat_vector.end());
      double lat_min(*iter);
      return LonLat(lon_min, lat_min);
    }
    else if (fid == FunctionId::Sum || fid == FunctionId::Integ)
    {
      double lon_sum = std::accumulate(lon_vector.begin(), lon_vector.end(), 0.0);
      double lat_sum = std::accumulate(lat_vector.begin(), lat_vector.end(), 0.0);

      while (abs(lon_sum) > 180)
        lon_sum -= 180;
      while (abs(lat_sum) > 90)
        lat_sum -= 90;

      return LonLat(lon_sum, lat_sum);
    }
    else
    {
      std::stringstream ss;
      ss << "Function " << func.hash() << " can not be applied for a lonlat-coordinate!";
      throw Spine::Exception(BCP, ss.str().c_str());
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StatCalculator::operator()(const TimedValue& tv)
{
  try
  {
    if (boost::get<double>(&(tv.value)))
    {
      double d(boost::get<double>(tv.value));
      itsDataVector.push_back(Fmi::Stat::DataItem(tv.time.utc_time(), d));
    }
    else if (boost::get<int>(&(tv.value)))
    {
      double d(boost::get<int>(tv.value));
      itsDataVector.push_back(Fmi::Stat::DataItem(tv.time.utc_time(), d));
    }
    else
    {
      itsTimeSeries.push_back(tv);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

Value StatCalculator::getStatValue(const ParameterFunction& func, bool useWeights) const
{
  try
  {
    // if result set contains missing values, then 'nan'-flag in ParameterFunction determines if we
    // ignore nan values and do statistical calculations with existing values or return missing
    // value
    bool missingValuesPresent(false);
    for (const TimedValue& tv : itsTimeSeries)
    {
      if (boost::get<Spine::TimeSeries::None>(&(tv.value)))
      {
        missingValuesPresent = true;
        break;
      }
    }

    if (!func.isNanFunction() && missingValuesPresent)
      return Spine::TimeSeries::None();

    Value ret = Spine::TimeSeries::None();

    if (itsDataVector.size() > 0)
    {
      ret = getDoubleStatValue(func, useWeights);
    }
    else if (itsTimeSeries.size() > 0)
    {
      Value value(itsTimeSeries[0].value);

      if (boost::get<std::string>(&value))
      {
        ret = getStringStatValue(func);
      }
      else if (boost::get<boost::local_time::local_date_time>(&value))
      {
        ret = getLocalDateTimeStatValue(func);
      }
      else if (boost::get<LonLat>(&value))
      {
        ret = getLonLatStatValue(func);
      }
    }
    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// returns aggregation indexes for each timestep
// first member in std::pair contains index behind the timestep
// second member in std::pair contains index ahead/after the timestep
std::vector<std::pair<int, int> > get_aggregation_indexes(const ParameterFunction& paramfunc,
                                                          const TimeSeries& ts)
{
  try
  {
    std::vector<std::pair<int, int> > agg_indexes;

    unsigned int agg_interval_behind(paramfunc.getAggregationIntervalBehind());
    unsigned int agg_interval_ahead(paramfunc.getAggregationIntervalAhead());
    std::size_t row_count = ts.size();

    for (unsigned int i = 0; i < row_count; i++)
    {
      std::pair<int, int> index_item(make_pair(-1, -1));

      // interval behind
      index_item.first = i;
      for (int j = i - 1; j >= 0; j--)
      {
        time_duration dur(ts[i].time - ts[j].time);
        if (dur.total_seconds() <= boost::numeric_cast<int>(agg_interval_behind * 60))
          index_item.first = j;
        else
          break;
      }

      // interval ahead
      index_item.second = i;
      for (unsigned int j = i + 1; j < row_count; j++)
      {
        time_duration dur(ts[j].time - ts[i].time);
        if (dur.total_seconds() <= boost::numeric_cast<int>(agg_interval_ahead * 60))
          index_item.second = j;
        else
          break;
      }
      agg_indexes.push_back(index_item);
    }

    return agg_indexes;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

TimeSeries area_aggregate(const TimeSeriesGroup& ts_group, const ParameterFunction& func)
{
  try
  {
    TimeSeries ret;

    if (ts_group.empty())
    {
      return ret;
    }

    // all time series in the result container are same length, so we use length of the first
    size_t ts_size(ts_group[0].timeseries.size());

    // iterate through timesteps
    for (size_t i = 0; i < ts_size; i++)
    {
      StatCalculator statcalculator;

      // iterate through locations
      for (size_t k = 0; k < ts_group.size(); k++)
      {
        statcalculator(ts_group[k].timeseries[i]);
      }
      // take timestamps from first location (they are same for all locations inside area)
      boost::local_time::local_date_time timestamp(ts_group[0].timeseries[i].time);

      ret.push_back(TimedValue(timestamp, statcalculator.getStatValue(func, false)));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

TimeSeriesPtr time_aggregate(const TimeSeries& ts, const ParameterFunction& func)
{
  try
  {
    TimeSeriesPtr ret(new TimeSeries());

    std::vector<std::pair<int, int> > agg_indexes = get_aggregation_indexes(func, ts);

    for (std::size_t i = 0; i < ts.size(); i++)
    {
      int agg_index_start(agg_indexes[i].first);
      int agg_index_end(agg_indexes[i].second);

      if (agg_index_start < 0 || agg_index_end < 0)
      {
        ret->push_back(TimedValue(ts[i].time, Spine::TimeSeries::None()));
        continue;
      }

      StatCalculator statcalculator;
      for (int k = agg_index_start; k <= agg_index_end; k++)
      {
        statcalculator(ts[k]);
      }
      ret->push_back(TimedValue(ts[i].time, statcalculator.getStatValue(func, true)));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

TimeSeriesGroupPtr time_aggregate(const TimeSeriesGroup& ts_group, const ParameterFunction& func)
{
  try
  {
    TimeSeriesGroupPtr ret(new TimeSeriesGroup());

    // iterate through locations
    for (size_t i = 0; i < ts_group.size(); i++)
    {
      TimeSeries ts(ts_group[i].timeseries);
      TimeSeriesPtr aggregated_timeseries(time_aggregate(ts, func));
      ret->push_back(LonLatTimeSeries(ts_group[i].lonlat, *aggregated_timeseries));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// only time-aggregation possible here
TimeSeriesPtr aggregate(const TimeSeries& ts, const ParameterFunctions& pf)
{
  try
  {
    TimeSeriesPtr ret(new TimeSeries);

    if (pf.innerFunction.type() == FunctionType::TimeFunction)
      ret = time_aggregate(ts, pf.innerFunction);
    else if (pf.outerFunction.type() == FunctionType::TimeFunction)
      ret = time_aggregate(ts, pf.outerFunction);
    else
      *ret = ts;

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

TimeSeriesGroupPtr aggregate(const TimeSeriesGroup& ts_group, const ParameterFunctions& pf)
{
  try
  {
    TimeSeriesGroupPtr ret(new TimeSeriesGroup);

    if (ts_group.empty())
    {
      return ret;
    }

    if (pf.outerFunction.type() == FunctionType::TimeFunction &&
        pf.innerFunction.type() == FunctionType::AreaFunction)
    {
#ifdef MYDEBUG
      cout << "time-area aggregation" << endl;
#endif

      // 1) do area aggregation
      TimeSeries area_aggregated_vector = area_aggregate(ts_group, pf.innerFunction);

      // 2) do time aggregation
      TimeSeriesPtr ts = time_aggregate(area_aggregated_vector, pf.outerFunction);

      ret->push_back(LonLatTimeSeries(ts_group[0].lonlat, *ts));
    }
    else if (pf.outerFunction.type() == FunctionType::AreaFunction &&
             pf.innerFunction.type() == FunctionType::TimeFunction)
    {
#ifdef MYDEBUG
      cout << "area-time aggregation" << endl;
#endif
      // 1) do time aggregation
      TimeSeriesGroupPtr time_aggregated_result = time_aggregate(ts_group, pf.innerFunction);

      // 2) do area aggregation
      TimeSeries ts = area_aggregate(*time_aggregated_result, pf.outerFunction);

      ret->push_back(LonLatTimeSeries(ts_group[0].lonlat, ts));
    }
    else if (pf.innerFunction.type() == FunctionType::AreaFunction)
    {
#ifdef MYDEBUG
      cout << "area aggregation" << endl;
#endif
      // 1) do area aggregation
      TimeSeries area_aggregated_vector = area_aggregate(ts_group, pf.innerFunction);

      ret->push_back(LonLatTimeSeries(ts_group[0].lonlat, area_aggregated_vector));
    }
    else if (pf.innerFunction.type() == FunctionType::TimeFunction)
    {
#ifdef MYDEBUG
      cout << "time aggregation" << endl;
#endif
      // 1) do time aggregation
      ret = time_aggregate(ts_group, pf.innerFunction);
    }
    else
    {
#ifdef MYDEBUG
      cout << "no aggregation" << endl;
#endif
      *ret = ts_group;
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet
