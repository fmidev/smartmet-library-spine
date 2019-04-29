// ======================================================================
/*!
 * \brief Regression tests for TimeSeriesAggregator
 */
// ======================================================================

#include "TimeSeriesAggregator.h"
#include "TimeSeriesGenerator.h"
#include "TimeSeriesOutput.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/foreach.hpp>
#include <regression/tframe.h>

namespace bp = boost::posix_time;
namespace bg = boost::gregorian;
namespace bl = boost::local_time;
namespace ts = SmartMet::Spine::TimeSeries;

// Protection against namespace tests
namespace TimeSeriesAggregatorTest
{
ts::TimeSeries generate_timeseries(bool add_missing_value = false)
{
  using namespace SmartMet;

  bl::time_zone_ptr zone(new bl::posix_time_zone("EET+2"));

  ts::TimeSeries timeseries;

  timeseries.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 1.0));
  timeseries.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 2.0));
  timeseries.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 3.0));
  if (add_missing_value)
    timeseries.push_back(ts::TimedValue(
        bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), ts::None()));
  else
    timeseries.push_back(ts::TimedValue(
        bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 4.0));
  timeseries.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 5.0));

  return timeseries;
}

ts::TimeSeriesGroup generate_timeseries_group()
{
  using namespace SmartMet;

  bl::time_zone_ptr zone(new bl::posix_time_zone("EET+2"));

  ts::TimeSeries timeseries_helsinki;

  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 1.0));
  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 6.0));
  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 11.0));
  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 16.0));
  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 21.0));

  ts::TimeSeries timeseries_tampere;

  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 2.0));
  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 7.0));
  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 12.0));
  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 17.0));
  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 22.0));

  ts::TimeSeries timeseries_oulu;

  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 3.0));
  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 8.0));
  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 13.0));
  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 18.0));
  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 23.0));

  ts::TimeSeries timeseries_kuopio;

  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 4.0));
  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 9.0));
  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 14.0));
  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 19.0));
  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 24.0));

  ts::TimeSeries timeseries_turku;

  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 5.0));
  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 10.0));
  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 15.0));
  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 20.0));
  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 25.0));

  ts::LonLatTimeSeries lonlat_ts_helsinki(ts::LonLat(24.9375, 60.1718), timeseries_helsinki);
  ts::LonLatTimeSeries lonlat_ts_tampere(ts::LonLat(23.7667, 61.5000), timeseries_tampere);
  ts::LonLatTimeSeries lonlat_ts_oulu(ts::LonLat(25.4667, 65.0167), timeseries_oulu);
  ts::LonLatTimeSeries lonlat_ts_kuopio(ts::LonLat(27.6783, 62.8925), timeseries_kuopio);
  ts::LonLatTimeSeries lonlat_ts_turku(ts::LonLat(22.2667, 60.4500), timeseries_turku);

  ts::TimeSeriesGroup timeseries_grp;
  timeseries_grp.push_back(lonlat_ts_helsinki);
  timeseries_grp.push_back(lonlat_ts_tampere);
  timeseries_grp.push_back(lonlat_ts_oulu);
  timeseries_grp.push_back(lonlat_ts_kuopio);
  timeseries_grp.push_back(lonlat_ts_turku);

  return timeseries_grp;
}

ts::TimeSeriesGroup generate_timeseries_group_nans()
{
  using namespace SmartMet;

  bl::time_zone_ptr zone(new bl::posix_time_zone("EET+2"));

  ts::TimeSeries timeseries_helsinki;

  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 1.0));
  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 6.0));
  timeseries_helsinki.push_back(
      ts::TimedValue(bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone),
                     SmartMet::Spine::TimeSeries::None()));
  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 16.0));
  timeseries_helsinki.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 21.0));

  ts::TimeSeries timeseries_tampere;

  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 2.0));
  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 7.0));
  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 17.0));
  timeseries_tampere.push_back(
      ts::TimedValue(bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone),
                     SmartMet::Spine::TimeSeries::None()));
  timeseries_tampere.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 22.0));

  ts::TimeSeries timeseries_oulu;

  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 3.0));
  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 8.0));
  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 13.0));
  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 18.0));
  timeseries_oulu.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 23.0));

  ts::TimeSeries timeseries_kuopio;

  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 4.0));
  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 9.0));
  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 14.0));
  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 19.0));
  timeseries_kuopio.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 24.0));

  ts::TimeSeries timeseries_turku;

  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(22)), zone), 5.0));
  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(23)), zone), 10.0));
  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(24)), zone), 15.0));
  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(25)), zone), 20.0));
  timeseries_turku.push_back(ts::TimedValue(
      bl::local_date_time(bp::ptime(bg::date(2015, 9, 2), bp::hours(26)), zone), 25.0));

  ts::LonLatTimeSeries lonlat_ts_helsinki(ts::LonLat(24.9375, 60.1718), timeseries_helsinki);
  ts::LonLatTimeSeries lonlat_ts_tampere(ts::LonLat(23.7667, 61.5000), timeseries_tampere);
  ts::LonLatTimeSeries lonlat_ts_oulu(ts::LonLat(25.4667, 65.0167), timeseries_oulu);
  ts::LonLatTimeSeries lonlat_ts_kuopio(ts::LonLat(27.6783, 62.8925), timeseries_kuopio);
  ts::LonLatTimeSeries lonlat_ts_turku(ts::LonLat(22.2667, 60.4500), timeseries_turku);

  ts::TimeSeriesGroup timeseries_grp;
  timeseries_grp.push_back(lonlat_ts_helsinki);
  timeseries_grp.push_back(lonlat_ts_tampere);
  timeseries_grp.push_back(lonlat_ts_oulu);
  timeseries_grp.push_back(lonlat_ts_kuopio);
  timeseries_grp.push_back(lonlat_ts_turku);

  return timeseries_grp;
}

template <typename T>
std::string to_string(const T& result_set)
{
  std::stringstream ret_ss;

  ret_ss << result_set;

  return ret_ss.str();
}

ts::TimeSeriesPtr execute_time_area_aggregation_function_with_range(
    SmartMet::Spine::FunctionId fid_time,
    SmartMet::Spine::FunctionId fid_area,
    unsigned int aggIntervalBehind,
    unsigned int aggIntervalAhead,
    double lowerLimit,
    double upperLimit,
    bool add_missing_value = false)
{
  using namespace SmartMet::Spine;

  ts::TimeSeries timeseries(generate_timeseries(add_missing_value));

  ParameterFunction pfInner(fid_time, FunctionType::TimeFunction, lowerLimit, upperLimit);
  pfInner.setAggregationIntervalBehind(aggIntervalBehind);
  pfInner.setAggregationIntervalAhead(aggIntervalAhead);
  ParameterFunctions pfs;
  pfs.innerFunction = pfInner;
  ParameterFunction pfOuter(fid_area, FunctionType::AreaFunction);
  pfs.outerFunction = pfOuter;

  ts::TimeSeriesPtr aggregated_timeseries = ts::aggregate(timeseries, pfs);

  return aggregated_timeseries;
}

ts::TimeSeriesPtr execute_area_time_aggregation_function_with_range(
    SmartMet::Spine::FunctionId fid_time,
    SmartMet::Spine::FunctionId fid_area,
    unsigned int aggIntervalBehind,
    unsigned int aggIntervalAhead,
    double lowerLimit,
    double upperLimit,
    bool add_missing_value = false)
{
  using namespace SmartMet::Spine;

  ts::TimeSeries timeseries(generate_timeseries(add_missing_value));

  ParameterFunction pfInner(fid_area, FunctionType::AreaFunction, lowerLimit, upperLimit);
  ParameterFunctions pfs;
  pfs.innerFunction = pfInner;
  ParameterFunction pfOuter(fid_time, FunctionType::TimeFunction);
  pfOuter.setAggregationIntervalBehind(aggIntervalBehind);
  pfOuter.setAggregationIntervalAhead(aggIntervalAhead);
  pfs.outerFunction = pfOuter;

  ts::TimeSeriesPtr aggregated_timeseries = ts::aggregate(timeseries, pfs);

  return aggregated_timeseries;
}

ts::TimeSeriesPtr execute_time_aggregation_function_with_range(SmartMet::Spine::FunctionId fid,
                                                               unsigned int aggIntervalBehind,
                                                               unsigned int aggIntervalAhead,
                                                               double lowerLimit,
                                                               double upperLimit,
                                                               bool add_missing_value = false)
{
  using namespace SmartMet::Spine;

  ts::TimeSeries timeseries(generate_timeseries(add_missing_value));

  ParameterFunction pf(fid, FunctionType::TimeFunction, lowerLimit, upperLimit);
  pf.setAggregationIntervalBehind(aggIntervalBehind);
  pf.setAggregationIntervalAhead(aggIntervalAhead);
  ParameterFunctions pfs;
  pfs.innerFunction = pf;
  ts::Value missing_value("nan");

  ts::TimeSeriesPtr aggregated_timeseries = ts::aggregate(timeseries, pfs);

  return aggregated_timeseries;
}

ts::TimeSeriesPtr execute_time_aggregation_function(SmartMet::Spine::FunctionId fid,
                                                    unsigned int aggIntervalBehind,
                                                    unsigned int aggIntervalAhead,
                                                    bool add_missing_value = false)
{
  using namespace SmartMet::Spine;

  ts::TimeSeries timeseries(generate_timeseries(add_missing_value));

  ParameterFunction pf(fid, FunctionType::TimeFunction);
  pf.setAggregationIntervalBehind(aggIntervalBehind);
  pf.setAggregationIntervalAhead(aggIntervalAhead);
  // lower limit (3) and upper limit (4) are used by Percentage and Count functions
  if (fid == SmartMet::Spine::FunctionId::Percentage || fid == SmartMet::Spine::FunctionId::Count)
    pf.setLimits(3.0, 4.0);
  ParameterFunctions pfs;
  pfs.innerFunction = pf;
  ts::Value missing_value("nan");

  ts::TimeSeriesPtr aggregated_timeseries = ts::aggregate(timeseries, pfs);

  return aggregated_timeseries;
}

ts::TimeSeriesGroupPtr execute_area_aggregation_function(SmartMet::Spine::FunctionId fid)
{
  using namespace SmartMet::Spine;

  ts::TimeSeriesGroup timeseries_grp(generate_timeseries_group());

  ParameterFunction pf(fid, FunctionType::AreaFunction);
  // lower limit (14) and upper limit (22) are used by Percentage and Count functions
  if (fid == SmartMet::Spine::FunctionId::Percentage || fid == SmartMet::Spine::FunctionId::Count)
    pf.setLimits(14.0, 22.0);

  ParameterFunctions pfs;
  pfs.innerFunction = pf;
  ts::Value missing_value("nan");

  ts::TimeSeriesGroupPtr aggregated_timeseries_grp = ts::aggregate(timeseries_grp, pfs);

  return aggregated_timeseries_grp;
}

ts::TimeSeriesGroupPtr execute_area_aggregation_function_with_range(SmartMet::Spine::FunctionId fid,
                                                                    double lowerLimit,
                                                                    double upperLimit)
{
  using namespace SmartMet::Spine;

  ts::TimeSeriesGroup timeseries_grp(generate_timeseries_group());

  ParameterFunction pf(fid, FunctionType::AreaFunction, lowerLimit, upperLimit);
  ParameterFunctions pfs;
  pfs.innerFunction = pf;
  ts::Value missing_value("nan");

  ts::TimeSeriesGroupPtr aggregated_timeseries_grp = ts::aggregate(timeseries_grp, pfs);

  return aggregated_timeseries_grp;
}

ts::TimeSeriesGroupPtr execute_area_aggregation_function_nans(SmartMet::Spine::FunctionId fid)
{
  using namespace SmartMet::Spine;

  ts::TimeSeriesGroup timeseries_grp(generate_timeseries_group_nans());

  ParameterFunction pf(fid, FunctionType::AreaFunction);
  // lower limit (14) and upper limit (22) are used by Percentage and Count functions
  if (fid == SmartMet::Spine::FunctionId::Percentage || fid == SmartMet::Spine::FunctionId::Count)
    pf.setLimits(14.0, 22.0);
  ParameterFunctions pfs;
  pfs.innerFunction = pf;
  ts::Value missing_value("nan");

  ts::TimeSeriesGroupPtr aggregated_timeseries_grp = ts::aggregate(timeseries_grp, pfs);

  return aggregated_timeseries_grp;
}

ts::TimeSeriesGroupPtr execute_area_aggregation_function_with_range_nans(
    SmartMet::Spine::FunctionId fid, double lowerLimit, double upperLimit)
{
  using namespace SmartMet::Spine;

  ts::TimeSeriesGroup timeseries_grp(generate_timeseries_group_nans());

  ParameterFunction pf(fid, FunctionType::AreaFunction, lowerLimit, upperLimit);
  ParameterFunctions pfs;
  pfs.innerFunction = pf;
  ts::Value missing_value("nan");

  ts::TimeSeriesGroupPtr aggregated_timeseries_grp = ts::aggregate(timeseries_grp, pfs);

  return aggregated_timeseries_grp;
}

ts::TimeSeriesGroupPtr execute_time_area_aggregation_function(SmartMet::Spine::FunctionId time_fid,
                                                              SmartMet::Spine::FunctionId area_fid,
                                                              unsigned int aggIntervalBehind,
                                                              unsigned int aggIntervalAhead)
{
  using namespace SmartMet::Spine;

  ts::TimeSeriesGroup timeseries_grp(generate_timeseries_group());

  ParameterFunction time_pf(time_fid, FunctionType::TimeFunction);
  time_pf.setAggregationIntervalBehind(aggIntervalBehind);
  time_pf.setAggregationIntervalAhead(aggIntervalAhead);
  // lower limit (3) and upper limit (4) are used by Percentage and Count functions
  if (time_fid == SmartMet::Spine::FunctionId::Percentage ||
      time_fid == SmartMet::Spine::FunctionId::Count)
    time_pf.setLimits(3.0, 4.0);

  ParameterFunctions time_pfs;
  time_pfs.innerFunction = time_pf;
  ts::Value missing_value("nan");

  ts::TimeSeriesGroupPtr time_aggregated_grp = ts::aggregate(timeseries_grp, time_pfs);

  ParameterFunction area_pf(area_fid, FunctionType::AreaFunction);
  // lower limit (14) and upper limit (22) are used by Percentage and Count functions
  if (area_fid == SmartMet::Spine::FunctionId::Percentage ||
      area_fid == SmartMet::Spine::FunctionId::Count)
    area_pf.setLimits(14.0, 22.0);
  ParameterFunctions area_pfs;
  area_pfs.innerFunction = area_pf;

  ts::TimeSeriesGroupPtr area_aggregated_grp = ts::aggregate(*time_aggregated_grp, area_pfs);

  return area_aggregated_grp;
}

ts::TimeSeriesGroupPtr execute_area_time_aggregation_function(SmartMet::Spine::FunctionId area_fid,
                                                              SmartMet::Spine::FunctionId time_fid,
                                                              unsigned int aggIntervalBehind,
                                                              unsigned int aggIntervalAhead)
{
  using namespace SmartMet::Spine;

  ts::TimeSeriesGroup timeseries_grp(generate_timeseries_group());
  ts::Value missing_value("nan");

  ParameterFunction area_pf(area_fid, FunctionType::AreaFunction);
  // lower limit (14) and upper limit (22) are used by Percentage and Count functions
  if (area_fid == SmartMet::Spine::FunctionId::Percentage ||
      area_fid == SmartMet::Spine::FunctionId::Count)
    area_pf.setLimits(14.0, 22.0);

  ParameterFunctions area_pfs;
  area_pfs.innerFunction = area_pf;

  ts::TimeSeriesGroupPtr area_aggregated_grp = ts::aggregate(timeseries_grp, area_pfs);

  // lower limit (3) and upper limit (4) are used by Percentage and Count functions
  ParameterFunction time_pf(time_fid, FunctionType::TimeFunction);
  if (time_fid == SmartMet::Spine::FunctionId::Percentage ||
      time_fid == SmartMet::Spine::FunctionId::Count)
    time_pf.setLimits(3.0, 4.0);

  time_pf.setAggregationIntervalBehind(aggIntervalBehind);
  time_pf.setAggregationIntervalAhead(aggIntervalAhead);
  ParameterFunctions time_pfs;
  time_pfs.innerFunction = time_pf;

  ts::TimeSeriesGroupPtr time_aggregated_grp = ts::aggregate(*area_aggregated_grp, time_pfs);

  return time_aggregated_grp;
}

void min_t()
{
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Minimum, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1\n"
      "2015-Sep-03 01:00:00 EET -> 1\n"
      "2015-Sep-03 02:00:00 EET -> 1\n"
      "2015-Sep-03 03:00:00 EET -> 2\n"
      "2015-Sep-03 04:00:00 EET -> 3\n";

  if (test_result != expected_result)
    TEST_FAILED("Minimum-function test failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void min_t_with_range()
{
  // Include 2,3
  std::string test_result = to_string(*execute_time_aggregation_function_with_range(
      SmartMet::Spine::FunctionId::Minimum, 120, 0, 2.0, 3.0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> nan\n"
      "2015-Sep-03 01:00:00 EET -> 2\n"
      "2015-Sep-03 02:00:00 EET -> 2\n"
      "2015-Sep-03 03:00:00 EET -> 2\n"
      "2015-Sep-03 04:00:00 EET -> 3\n";

  if (test_result != expected_result)
    TEST_FAILED("Minimum-function with range test failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void max_t()
{
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Maximum, 0, 120));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 3\n"
      "2015-Sep-03 01:00:00 EET -> 4\n"
      "2015-Sep-03 02:00:00 EET -> 5\n"
      "2015-Sep-03 03:00:00 EET -> 5\n"
      "2015-Sep-03 04:00:00 EET -> 5\n";

  if (expected_result != test_result)
    TEST_FAILED("Max-function test failed. Result should be:\n" + expected_result + "\n not \n" +
                test_result);

  TEST_PASSED();
}

void max_t_with_range()
{
  // Incluee 3,4
  std::string test_result = to_string(*execute_time_aggregation_function_with_range(
      SmartMet::Spine::FunctionId::Maximum, 0, 120, 3, 4));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 3\n"
      "2015-Sep-03 01:00:00 EET -> 4\n"
      "2015-Sep-03 02:00:00 EET -> 4\n"
      "2015-Sep-03 03:00:00 EET -> 4\n"
      "2015-Sep-03 04:00:00 EET -> nan\n";

  if (expected_result != test_result)
    TEST_FAILED("Max-function with range test failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void mean_t()
{
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Mean, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1\n"
      "2015-Sep-03 01:00:00 EET -> 1.5\n"
      "2015-Sep-03 02:00:00 EET -> 2\n"
      "2015-Sep-03 03:00:00 EET -> 3\n"
      "2015-Sep-03 04:00:00 EET -> 4\n";

  if (expected_result != test_result)
    TEST_FAILED("Mean-function test failed. Result should be:\n" + expected_result + "\n not \n" +
                test_result);

  TEST_PASSED();
}

void mean_t_with_range()
{
  // Include 2,3,4
  std::string test_result = to_string(*execute_time_aggregation_function_with_range(
      SmartMet::Spine::FunctionId::Mean, 120, 0, 2.0, 4.0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> nan\n"
      "2015-Sep-03 01:00:00 EET -> 2\n"
      "2015-Sep-03 02:00:00 EET -> 2.5\n"
      "2015-Sep-03 03:00:00 EET -> 3\n"
      "2015-Sep-03 04:00:00 EET -> 3.5\n";

  if (expected_result != test_result)
    TEST_FAILED("Mean-function with range test failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void mean_a_t_with_range()
{
  // Include 1,2,3
  std::string test_result = to_string(*execute_time_area_aggregation_function_with_range(
      SmartMet::Spine::FunctionId::Mean, SmartMet::Spine::FunctionId::Mean, 120, 0, 1.0, 3.0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1\n"
      "2015-Sep-03 01:00:00 EET -> 1.5\n"
      "2015-Sep-03 02:00:00 EET -> 2\n"
      "2015-Sep-03 03:00:00 EET -> 2.5\n"
      "2015-Sep-03 04:00:00 EET -> 3\n";

  if (expected_result != test_result)
    TEST_FAILED("Mean area and time function with range test failed. Result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void mean_t_a_with_range()
{
  // Include 1,2,3
  std::string test_result = to_string(*execute_area_time_aggregation_function_with_range(
      SmartMet::Spine::FunctionId::Mean, SmartMet::Spine::FunctionId::Mean, 120, 0, 1.0, 3.0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1\n"
      "2015-Sep-03 01:00:00 EET -> 1.5\n"
      "2015-Sep-03 02:00:00 EET -> 2\n"
      "2015-Sep-03 03:00:00 EET -> nan\n"
      "2015-Sep-03 04:00:00 EET -> nan\n";

  if (expected_result != test_result)
    TEST_FAILED("Mean time and area function with range test failed. Result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void median_t()
{
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Median, 240, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1\n"
      "2015-Sep-03 01:00:00 EET -> 1.5\n"
      "2015-Sep-03 02:00:00 EET -> 2\n"
      "2015-Sep-03 03:00:00 EET -> 2.5\n"
      "2015-Sep-03 04:00:00 EET -> 3\n";

  if (expected_result != test_result)
    TEST_FAILED("Median-function test failed. Result should be:\n" + expected_result + "\n not \n" +
                test_result);

  TEST_PASSED();
}

void median_t_with_range()
{
  // Include 2,3,4
  std::string test_result = to_string(*execute_time_aggregation_function_with_range(
      SmartMet::Spine::FunctionId::Median, 240, 0, 2, 4));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> nan\n"
      "2015-Sep-03 01:00:00 EET -> 2\n"
      "2015-Sep-03 02:00:00 EET -> 2.5\n"
      "2015-Sep-03 03:00:00 EET -> 3\n"
      "2015-Sep-03 04:00:00 EET -> 3\n";

  if (expected_result != test_result)
    TEST_FAILED("Median-function with range test failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void sum_t()
{
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Sum, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1\n"
      "2015-Sep-03 01:00:00 EET -> 3\n"
      "2015-Sep-03 02:00:00 EET -> 6\n"
      "2015-Sep-03 03:00:00 EET -> 9\n"
      "2015-Sep-03 04:00:00 EET -> 12\n";

  if (expected_result != test_result)
    TEST_FAILED("Sum-function test failed. Expected result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void integ_t()
{
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Integ, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 0.000277778\n"
      "2015-Sep-03 01:00:00 EET -> 1.5\n"
      "2015-Sep-03 02:00:00 EET -> 4\n"
      "2015-Sep-03 03:00:00 EET -> 6\n"
      "2015-Sep-03 04:00:00 EET -> 8\n";

  if (expected_result != test_result)
    TEST_FAILED("Integ-function test failed. Expected result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void stddev_t()
{
  std::string test_result = to_string(
      *execute_time_aggregation_function(SmartMet::Spine::FunctionId::StandardDeviation, 240, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 0\n"
      "2015-Sep-03 01:00:00 EET -> 0.5\n"
      "2015-Sep-03 02:00:00 EET -> 0.707107\n"
      "2015-Sep-03 03:00:00 EET -> 0.957427\n"
      "2015-Sep-03 04:00:00 EET -> 1.22474\n";

  if (expected_result != test_result)
    TEST_FAILED("StandardDeviation-function test failed. Expected result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void percentage_t()
{
  std::string test_result = to_string(
      *execute_time_aggregation_function(SmartMet::Spine::FunctionId::Percentage, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 0\n"
      "2015-Sep-03 01:00:00 EET -> 0\n"
      "2015-Sep-03 02:00:00 EET -> 25\n"
      "2015-Sep-03 03:00:00 EET -> 75\n"
      "2015-Sep-03 04:00:00 EET -> 75\n";

  if (expected_result != test_result)
    TEST_FAILED("Percentage-function test failed. Expected result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void count_t()
{
  // not count doesn't make much sense when aggregating over time, since weigths are used
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Count, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 0\n"
      "2015-Sep-03 01:00:00 EET -> 0\n"
      "2015-Sep-03 02:00:00 EET -> 1\n"
      "2015-Sep-03 03:00:00 EET -> 2\n"
      "2015-Sep-03 04:00:00 EET -> 2\n";

  if (expected_result != test_result)
    TEST_FAILED("Count-function test failed. Expected result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void change_t()
{
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Change, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 0\n"
      "2015-Sep-03 01:00:00 EET -> 1\n"
      "2015-Sep-03 02:00:00 EET -> 2\n"
      "2015-Sep-03 03:00:00 EET -> 2\n"
      "2015-Sep-03 04:00:00 EET -> 2\n";

  if (expected_result != test_result)
    TEST_FAILED("Change-function test failed. Expected result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void trend_t()
{
  std::string test_result =
      to_string(*execute_time_aggregation_function(SmartMet::Spine::FunctionId::Trend, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 32700\n"
      "2015-Sep-03 01:00:00 EET -> 100\n"
      "2015-Sep-03 02:00:00 EET -> 66.6667\n"
      "2015-Sep-03 03:00:00 EET -> 66.6667\n"
      "2015-Sep-03 04:00:00 EET -> 66.6667\n";

  if (expected_result != test_result)
    TEST_FAILED("Trend-function test failed. Expected result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

// area aggregation
void min_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Minimum));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1\n"
      "2015-Sep-03 01:00:00 EET -> 6\n"
      "2015-Sep-03 02:00:00 EET -> 11\n"
      "2015-Sep-03 03:00:00 EET -> 16\n"
      "2015-Sep-03 04:00:00 EET -> 21\n";

  if (test_result != expected_result)
    TEST_FAILED("Minimum-function test for area failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void max_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Maximum));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 5\n"
      "2015-Sep-03 01:00:00 EET -> 10\n"
      "2015-Sep-03 02:00:00 EET -> 15\n"
      "2015-Sep-03 03:00:00 EET -> 20\n"
      "2015-Sep-03 04:00:00 EET -> 25\n";

  if (test_result != expected_result)
    TEST_FAILED("Maximum-function test for area failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);
  TEST_PASSED();
}

void max_a_nan()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function_nans(SmartMet::Spine::FunctionId::Maximum));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 5\n"
      "2015-Sep-03 01:00:00 EET -> 10\n"
      "2015-Sep-03 02:00:00 EET -> nan\n"
      "2015-Sep-03 03:00:00 EET -> nan\n"
      "2015-Sep-03 04:00:00 EET -> 25\n";

  if (test_result != expected_result)
    TEST_FAILED("Maximum-function test for area failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void max_a_with_range_nan()
{
  // Include values between 5.0 and 15.0
  std::string test_result = to_string(*execute_area_aggregation_function_with_range_nans(
      SmartMet::Spine::FunctionId::Maximum, 4.0, 22.0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 5\n"
      "2015-Sep-03 01:00:00 EET -> 10\n"
      "2015-Sep-03 02:00:00 EET -> nan\n"
      "2015-Sep-03 03:00:00 EET -> nan\n"
      "2015-Sep-03 04:00:00 EET -> 22\n";

  if (test_result != expected_result)
    TEST_FAILED("Maximum-function with range test for area failed. Result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void mean_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Mean));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 3\n"
      "2015-Sep-03 01:00:00 EET -> 8\n"
      "2015-Sep-03 02:00:00 EET -> 13\n"
      "2015-Sep-03 03:00:00 EET -> 18\n"
      "2015-Sep-03 04:00:00 EET -> 23\n";

  if (test_result != expected_result)
    TEST_FAILED("Mean-function test for area failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void mean_a_with_range()
{
  // Inclue values between 3.0 and 15.0
  std::string test_result = to_string(
      *execute_area_aggregation_function_with_range(SmartMet::Spine::FunctionId::Mean, 3.0, 15.0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 4\n"
      "2015-Sep-03 01:00:00 EET -> 8\n"
      "2015-Sep-03 02:00:00 EET -> 13\n"
      "2015-Sep-03 03:00:00 EET -> nan\n"
      "2015-Sep-03 04:00:00 EET -> nan\n";

  if (test_result != expected_result)
    TEST_FAILED("Mean-function with_range test for area failed. Result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void median_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Median));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 3\n"
      "2015-Sep-03 01:00:00 EET -> 8\n"
      "2015-Sep-03 02:00:00 EET -> 13\n"
      "2015-Sep-03 03:00:00 EET -> 18\n"
      "2015-Sep-03 04:00:00 EET -> 23\n";

  if (test_result != expected_result)
    TEST_FAILED("Medin-function test for area failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void sum_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Sum));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 15\n"
      "2015-Sep-03 01:00:00 EET -> 40\n"
      "2015-Sep-03 02:00:00 EET -> 65\n"
      "2015-Sep-03 03:00:00 EET -> 90\n"
      "2015-Sep-03 04:00:00 EET -> 115\n";

  if (test_result != expected_result)
    TEST_FAILED("Sum-function test for area failed. Result should be:\n" + expected_result +
                "\n not \n" + test_result);

  TEST_PASSED();
}

void stddev_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::StandardDeviation));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1.41421\n"
      "2015-Sep-03 01:00:00 EET -> 1.41421\n"
      "2015-Sep-03 02:00:00 EET -> 1.41421\n"
      "2015-Sep-03 03:00:00 EET -> 1.41421\n"
      "2015-Sep-03 04:00:00 EET -> 1.41421\n";

  if (test_result != expected_result)
    TEST_FAILED("StandardDeviation-function test for area failed. Result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void percentage_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Percentage));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 0\n"
      "2015-Sep-03 01:00:00 EET -> 0\n"
      "2015-Sep-03 02:00:00 EET -> 40\n"
      "2015-Sep-03 03:00:00 EET -> 100\n"
      "2015-Sep-03 04:00:00 EET -> 40\n";

  if (expected_result != test_result)
    TEST_FAILED("Percentage-function test for area failed. Expected result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void count_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Count));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 0\n"
      "2015-Sep-03 01:00:00 EET -> 0\n"
      "2015-Sep-03 02:00:00 EET -> 2\n"
      "2015-Sep-03 03:00:00 EET -> 5\n"
      "2015-Sep-03 04:00:00 EET -> 2\n";

  if (expected_result != test_result)
    TEST_FAILED("Count-function test for area failed. Expected result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void change_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Change));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 4\n"
      "2015-Sep-03 01:00:00 EET -> 4\n"
      "2015-Sep-03 02:00:00 EET -> 4\n"
      "2015-Sep-03 03:00:00 EET -> 4\n"
      "2015-Sep-03 04:00:00 EET -> 4\n";

  if (expected_result != test_result)
    TEST_FAILED("Change-function test for area failed. Expected result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void trend_a()
{
  std::string test_result =
      to_string(*execute_area_aggregation_function(SmartMet::Spine::FunctionId::Trend));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 100\n"
      "2015-Sep-03 01:00:00 EET -> 100\n"
      "2015-Sep-03 02:00:00 EET -> 100\n"
      "2015-Sep-03 03:00:00 EET -> 100\n"
      "2015-Sep-03 04:00:00 EET -> 100\n";

  if (expected_result != test_result)
    TEST_FAILED("Trend-function test for area failed. Expected result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

// first time then area aggregation
void min_max_ta()
{
  std::string test_result = to_string(*execute_time_area_aggregation_function(
      SmartMet::Spine::FunctionId::Minimum, SmartMet::Spine::FunctionId::Maximum, 120, 0));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 5\n"
      "2015-Sep-03 01:00:00 EET -> 5\n"
      "2015-Sep-03 02:00:00 EET -> 5\n"
      "2015-Sep-03 03:00:00 EET -> 10\n"
      "2015-Sep-03 04:00:00 EET -> 15\n";

  if (test_result != expected_result)
    TEST_FAILED("Minimum-Maximum-function test for time and area failed. Result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

// first area then time aggregation
void min_max_at()
{
  std::string test_result = to_string(*execute_area_time_aggregation_function(
      SmartMet::Spine::FunctionId::Minimum, SmartMet::Spine::FunctionId::Maximum, 0, 120));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 11\n"
      "2015-Sep-03 01:00:00 EET -> 16\n"
      "2015-Sep-03 02:00:00 EET -> 21\n"
      "2015-Sep-03 03:00:00 EET -> 21\n"
      "2015-Sep-03 04:00:00 EET -> 21\n";

  if (test_result != expected_result)
    TEST_FAILED("Minimum-Maximum-function test for area and time failed. Result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

void min_t_missing_value()
{
  std::string test_result = to_string(
      *execute_time_aggregation_function(SmartMet::Spine::FunctionId::Minimum, 120, 0, true));

  std::string expected_result =
      "2015-Sep-03 00:00:00 EET -> 1\n"
      "2015-Sep-03 01:00:00 EET -> 1\n"
      "2015-Sep-03 02:00:00 EET -> 1\n"
      "2015-Sep-03 03:00:00 EET -> nan\n"
      "2015-Sep-03 04:00:00 EET -> nan\n";

  if (test_result != expected_result)
    TEST_FAILED("Minimum-function with missing value test failed. Result should be:\n" +
                expected_result + "\n not \n" + test_result);

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * The actual test suite
 */
// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test()
  {
    // time aggregation
    TEST(min_t);
    TEST(max_t);
    TEST(mean_t);
    TEST(median_t);
    TEST(sum_t);
    TEST(integ_t);
    TEST(stddev_t);
    TEST(percentage_t);
    TEST(count_t);
    TEST(change_t);
    TEST(trend_t);
    // area aggregation
    TEST(min_a);
    TEST(max_a);
    TEST(max_a_nan);
    TEST(mean_a);
    TEST(median_a);
    TEST(sum_a);
    TEST(stddev_a);
    TEST(percentage_a);
    TEST(count_a);
    TEST(change_a);
    TEST(trend_a);
    // first time the area aggregation
    TEST(min_max_ta);
    // first area then time aggregation
    TEST(min_max_at);
    // missing value in time series
    TEST(min_t_missing_value);
    // tests functions with inclusive ranges
    TEST(min_t_with_range);
    TEST(max_t_with_range);
    TEST(mean_t_with_range);
    TEST(median_t_with_range);
    TEST(mean_a_with_range);
    TEST(max_a_with_range_nan);

    TEST(mean_a_t_with_range);

    TEST(mean_t_a_with_range);
  }
};

}  // namespace TimeSeriesAggregatorTest

//! The main program
int main()
{
  using namespace std;
  cout << endl << "TimeSeriesAggregator tester" << endl << "==========================" << endl;
  TimeSeriesAggregatorTest::tests t;
  return t.run();
}
