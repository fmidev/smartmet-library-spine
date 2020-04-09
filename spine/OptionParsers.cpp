#include "OptionParsers.h"
#include "Convenience.h"
#include "Exception.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <macgyver/StringConversion.h>

#include <stdexcept>

const int default_timestep = 60;

namespace SmartMet
{
namespace Spine
{
namespace OptionParsers
{
namespace
{
int get_parameter_index(const ParameterList& parameters, const std::string& paramname)
{
  for (unsigned int i = 0; i < parameters.size(); i++)
    if (parameters.at(i).name() == paramname)
      return i;

  return -1;
}
}  // anonymous namespace

void ParameterOptions::expandParameter(const std::string& paramname)
{
  // Expand parameter in index
  int expand_index = get_parameter_index(itsParameters, paramname);

  if (expand_index > -1)
  {
    const ParameterFunctions& expand_func = itsParameterFunctions.at(expand_index).functions;

    std::vector<Parameter> dataSourceParams;
    std::vector<ParameterAndFunctions> dataSourceParameterFunctions;
    for (const auto& p : itsParameters)
    {
      if (p.type() == SmartMet::Spine::Parameter::Type::Data ||
          p.type() == SmartMet::Spine::Parameter::Type::Landscaped)
      {
        std::string name = p.name() + "_data_source";
        Fmi::ascii_tolower(name);

        Parameter param(name, p.type(), p.number());
        if (p.getSensorNumber())
          param.setSensorNumber(*(p.getSensorNumber()));
        ParameterAndFunctions paramfunc(param, expand_func);
        dataSourceParams.push_back(param);
        dataSourceParameterFunctions.push_back(paramfunc);
      }
    }

    if (!dataSourceParams.empty())
    {
      // Replace the original parameter with expanded list of parameters
      itsParameters.erase(itsParameters.begin() + expand_index);
      itsParameters.insert(
          itsParameters.begin() + expand_index, dataSourceParams.begin(), dataSourceParams.end());
      itsParameterFunctions.erase(itsParameterFunctions.begin() + expand_index);
      itsParameterFunctions.insert(itsParameterFunctions.begin() + expand_index,
                                   dataSourceParameterFunctions.begin(),
                                   dataSourceParameterFunctions.end());
    }
  }
}

void ParameterOptions::add(const Parameter& theParam)
{
  itsParameters.push_back(theParam);
  itsParameterFunctions.push_back(ParameterAndFunctions(theParam, ParameterFunctions()));
}

void ParameterOptions::add(const Parameter& theParam, const ParameterFunctions& theParamFunctions)
{
  itsParameters.push_back(theParam);
  itsParameterFunctions.push_back(ParameterAndFunctions(theParam, theParamFunctions));
}

ParameterOptions parseParameters(const HTTP::Request& theReq)
{
  try
  {
    ParameterOptions options;

    // Get the option string
    std::string opt = required_string(theReq.getParameter("param"), "Option param is required");

    // Protect against empty selection
    if (opt.empty())
      throw Spine::Exception(BCP, "param option is empty");

    // Split

    std::vector<std::string> names;
    boost::algorithm::split(names, opt, boost::algorithm::is_any_of(","));

    // Validate and convert

    for (const std::string& name : names)
    {
      options.add(ParameterFactory::instance().parse(name));
    }

    return options;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse time series generation options.
 *
 * Note: If you add a new parameter, please do not forget to change
 *       the timeseries-plugin hash_value method accordinly.
 */
// ----------------------------------------------------------------------

TimeSeriesGeneratorOptions parseTimes(const HTTP::Request& theReq)
{
  try
  {
    boost::posix_time::ptime now = optional_time(theReq.getParameter("now"),
                                                 boost::posix_time::second_clock::universal_time());

    TimeSeriesGeneratorOptions options(now);

    // We first parse the options which imply the TimeMode
    // because the behaviour of some other options depend on it

    // Parse hour option
    if (theReq.getParameter("hour"))
    {
      options.mode = TimeSeriesGeneratorOptions::FixedTimes;

      const std::string hours = required_string(theReq.getParameter("hour"), "");

      std::vector<std::string> parts;
      boost::algorithm::split(parts, hours, boost::algorithm::is_any_of(","));
      for (const std::string& part : parts)
      {
        int hour = Fmi::stoi(part);
        if (hour < 0 || hour > 23)
          throw Spine::Exception(BCP, "Invalid hour selection '" + part + "'!");

        options.timeList.insert(hour * 100);
      }
    }

    // Parse time option
    if (theReq.getParameter("time"))
    {
      options.mode = TimeSeriesGeneratorOptions::FixedTimes;
      const std::string times = required_string(theReq.getParameter("time"), "");

      std::vector<std::string> parts;
      boost::algorithm::split(parts, times, boost::algorithm::is_any_of(","));

      for (const std::string& part : parts)
      {
        int th = Fmi::stoi(part);
        if (th < 0 || th > 2359)
          throw Spine::Exception(BCP, "Invalid time selection '" + part + "'!");

        options.timeList.insert(th);
      }
    }

    // Timestep should be parsed last so we can check if the
    // defaults have been changed

    if (theReq.getParameter("timestep"))
    {
      if (options.mode != TimeSeriesGeneratorOptions::TimeSteps)
        throw Spine::Exception(
            BCP, "Cannot use timestep option when another time mode is implied by another option");

      const std::string step = optional_string(theReq.getParameter("timestep"), "");

      if (step == "data" || step == "all")
      {
        options.mode = TimeSeriesGeneratorOptions::DataTimes;
        options.timeStep = 0;
      }
      else if (step == "graph")
      {
        options.mode = TimeSeriesGeneratorOptions::GraphTimes;
        options.timeStep = 0;
      }
      else if (step != "current")
      {
        options.mode = TimeSeriesGeneratorOptions::TimeSteps;
        int num = duration_string_to_minutes(step);

        if (num < 0)
          throw Spine::Exception(BCP, "The 'timestep' option cannot be negative!");

        if (num > 0 && 1440 % num != 0)
          throw Spine::Exception(BCP,
                                 "Timestep must be a divisor of 24*60 or zero for all timesteps!");

        options.timeStep = num;
      }
    }

    // TIMEMODE HAS NOW BEEN DETERMINED

    // The number of timesteps is a spine option for various modes,
    // including DataTimes and GraphTimes

    if (theReq.getParameter("timesteps"))
    {
      const std::string steps = optional_string(theReq.getParameter("timesteps"), "");

      int num = Fmi::stoi(steps);
      if (num < 0)
        throw Spine::Exception(BCP, "The 'timesteps' option cannot be negative!");

      options.timeSteps = num;
    }

    // starttime option may modify "now" which is the default
    // value set by the constructor

    if (theReq.getParameter("starttime"))
    {
      std::string stamp = required_string(theReq.getParameter("starttime"), "");

      if (stamp == "data")
        options.startTimeData = true;

      else
      {
        options.startTime = Fmi::TimeParser::parse(stamp);
        options.startTimeUTC = Fmi::TimeParser::looks_utc(stamp);
      }
    }
    else
    {
      options.startTimeUTC = true;  // generate from "now" in all locations
    }

    // startstep must be handled after starttime and timestep options

    if (theReq.getParameter("startstep"))
    {
      int startstep = optional_int(theReq.getParameter("startstep"), 0);

      if (startstep < 0)
        throw Spine::Exception(BCP, "The 'startstep' option cannot be negative!");
      if (startstep > 10000)
        throw Spine::Exception(BCP, "Too large 'startstep' value!");

      int timestep = (options.timeStep ? *options.timeStep : default_timestep);

      options.startTime +=
          boost::posix_time::minutes(boost::numeric_cast<unsigned int>(startstep) * timestep);
    }

    // endtime must be handled last since it may depend on starttime,
    // timestep and timesteps options. The default is to set the end
    // time to the start time plus 24 hours.

    if (theReq.getParameter("endtime"))
    {
      std::string stamp = required_string(theReq.getParameter("endtime"), "");
      if (stamp != "now")
      {
        if (!!options.timeSteps)
          throw Spine::Exception(BCP, "Cannot specify 'timesteps' and 'endtime' simultaneously!");

        if (stamp == "data")
          options.endTimeData = true;
        else
        {
          // iso, sql, xml, timestamp obey tz, epoch is always UTC
          options.endTime = Fmi::TimeParser::parse(stamp);
          options.endTimeUTC = Fmi::TimeParser::looks_utc(stamp);
        }
      }
    }
    else if (!!options.timeSteps)
    {
      options.endTimeUTC = options.startTimeUTC;
      // If you give the number of timesteps, we must assume a default timestep unless you give
      // one
      if (!options.timeStep)
        options.timeStep = default_timestep;
      options.endTime =
          options.startTime + boost::posix_time::minutes(*options.timeStep * *options.timeSteps);
    }
    else
    {
      options.endTimeUTC = options.startTimeUTC;
      options.endTime = options.startTime + boost::posix_time::hours(24);
    }

    return options;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace OptionParsers
}  // namespace Spine
}  // namespace SmartMet
