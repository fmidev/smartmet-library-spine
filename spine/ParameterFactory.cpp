#include "ParameterFactory.h"
#include "Convenience.h"
#include "Parameters.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <stdexcept>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Print parameter and functions
 */
// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& out, const ParameterAndFunctions& paramfuncs)
{
  out << "parameter =\t" << paramfuncs.parameter << "\n"
      << "functions =\t" << paramfuncs.functions << "\n";
  return out;
}

int get_function_index(const std::string& theFunction)
{
  static const char* names[] = {"mean_a",
                                "mean_t",
                                "nanmean_a",
                                "nanmean_t",
                                "max_a",
                                "max_t",
                                "nanmax_a",
                                "nanmax_t",
                                "min_a",
                                "min_t",
                                "nanmin_a",
                                "nanmin_t",
                                "median_a",
                                "median_t",
                                "nanmedian_a",
                                "nanmedian_t",
                                "sum_a",
                                "sum_t",
                                "nansum_a",
                                "nansum_t",
                                "integ_a",
                                "integ_t",
                                "naninteg_a",
                                "naninteg_t",
                                "sdev_a",
                                "sdev_t",
                                "nansdev_a",
                                "nansdev_t",
                                "percentage_a",
                                "percentage_t",
                                "nanpercentage_a",
                                "nanpercentage_t",
                                "count_a",
                                "count_t",
                                "nancount_a",
                                "nancount_t",
                                "change_a",
                                "change_t",
                                "nanchange_a",
                                "nanchange_t",
                                "trend_a",
                                "trend_t",
                                "nantrend_a",
                                "nantrend_t",
                                "nearest_t",
                                "nannearest_t",
                                "interpolate_t",
                                "naninterpolate_t",
                                ""};

  std::string func_name(theFunction);

  // If ending is missing, add area aggregation ending
  if (func_name.find("_a") == std::string::npos && func_name.find("_t") == std::string::npos)
    func_name += "_a";

  for (unsigned int i = 0; strlen(names[i]) > 0; i++)
  {
    if (names[i] == func_name)
      return i;
  }

  return -1;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get an instance of the parameter factory (singleton)
 */
// ----------------------------------------------------------------------

const ParameterFactory& ParameterFactory::instance()
{
  try
  {
    static ParameterFactory factory;
    return factory;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the factory
 *
 * This is only called once by instance()
 */
// ----------------------------------------------------------------------

ParameterFactory::ParameterFactory() : converter()
{
  try
  {
    // We must make one query to make sure the converter is
    // initialized while the constructor is run thread safely
    // only once. Unfortunately the init method is private.

    converter.ToString(1);  // 1 == Pressure
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return name for the given parameter
 */
// ----------------------------------------------------------------------

std::string ParameterFactory::name(int number) const
{
  try
  {
    return converter.ToString(number);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return number for the given name
 */

int ParameterFactory::number(const std::string& name) const
{
  try
  {
    return converter.ToEnum(name);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a function id
 *
 * Throws if the name is not recognized.
 *
 * \param theFunction The name of the function
 * \return The respective enumerated value
 */
// ----------------------------------------------------------------------

FunctionId ParameterFactory::parse_function(const std::string& theFunction) const
{
  try
  {
    static FunctionId functions[] = {FunctionId::Mean,
                                     FunctionId::Mean,
                                     FunctionId::Mean,
                                     FunctionId::Mean,
                                     FunctionId::Maximum,
                                     FunctionId::Maximum,
                                     FunctionId::Maximum,
                                     FunctionId::Maximum,
                                     FunctionId::Minimum,
                                     FunctionId::Minimum,
                                     FunctionId::Minimum,
                                     FunctionId::Minimum,
                                     FunctionId::Median,
                                     FunctionId::Median,
                                     FunctionId::Median,
                                     FunctionId::Median,
                                     FunctionId::Sum,
                                     FunctionId::Sum,
                                     FunctionId::Sum,
                                     FunctionId::Sum,
                                     FunctionId::Sum,
                                     FunctionId::Integ,
                                     FunctionId::Sum,
                                     FunctionId::Integ,
                                     FunctionId::StandardDeviation,
                                     FunctionId::StandardDeviation,
                                     FunctionId::StandardDeviation,
                                     FunctionId::StandardDeviation,
                                     FunctionId::Percentage,
                                     FunctionId::Percentage,
                                     FunctionId::Percentage,
                                     FunctionId::Percentage,
                                     FunctionId::Count,
                                     FunctionId::Count,
                                     FunctionId::Count,
                                     FunctionId::Count,
                                     FunctionId::Change,
                                     FunctionId::Change,
                                     FunctionId::Change,
                                     FunctionId::Change,
                                     FunctionId::Trend,
                                     FunctionId::Trend,
                                     FunctionId::Trend,
                                     FunctionId::Trend,
                                     FunctionId::Nearest,
                                     FunctionId::Nearest,
                                     FunctionId::Interpolate,
                                     FunctionId::Interpolate};

    int function_index = get_function_index(theFunction);
    if (function_index >= 0)
      return functions[function_index];

    throw Fmi::Exception(BCP, "Unrecognized function name '" + theFunction + "'!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the function modifier from a function call string
 *
 * This parses strings of the following forms
 *
 *  - name
 *  - name[lo:hi]
 *
 * \param theString The function call to parse
 * \return The function modified alone (possibly empty string)
 */
// ----------------------------------------------------------------------
std::string ParameterFactory::extract_limits(const std::string& theString) const
{
  try
  {
    auto pos1 = theString.find('[');
    if (pos1 == std::string::npos)
      return "";

    auto pos2 = theString.find(']', pos1);
    if (pos2 == std::string::npos && pos2 != theString.size() - 1)
      throw Fmi::Exception(BCP, "Invalid function modifier in '" + theString + "'!");

    return theString.substr(pos1 + 1, pos2 - pos1 - 1);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract the function name from a function call string
 *
 * This parses strings of the following forms
 *
 *  - name
 *  - name[lo:hi]
 *
 * \param theString The function call to parse
 * \return The function name alone
 */
// ----------------------------------------------------------------------

std::string ParameterFactory::extract_function(const std::string& theString,
                                               double& theLowerLimit,
                                               double& theUpperLimit) const
{
  try
  {
    auto pos = theString.find('[');

    theLowerLimit = -std::numeric_limits<double>::max();
    theUpperLimit = std::numeric_limits<double>::max();

    if (pos == std::string::npos)
      return theString;

    auto function_name = theString.substr(0, pos);

    auto limits = extract_limits(theString);

    if (!limits.empty())
    {
      const auto pos = limits.find(':');

      if (pos == std::string::npos)
        throw Fmi::Exception(BCP, "Unrecognized modifier format '" + limits + "'!");

      auto lo = limits.substr(0, pos);
      auto hi = limits.substr(pos + 1);
      Fmi::trim(lo);
      Fmi::trim(hi);

      if (lo.empty() && hi.empty())
        throw Fmi::Exception(BCP, "Both lower and upper limit are missing from the modifier!");

      if (!lo.empty())
        theLowerLimit = Fmi::stod(lo);
      if (!hi.empty())
        theUpperLimit = Fmi::stod(hi);
    }

    return function_name;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Returns parameter name
 *
 * This function handles the aliases and returns same name for all aliases
 *
 *  - original_name
 *
 * \return The parameter name
 */
// ----------------------------------------------------------------------

std::string parse_parameter_name(const std::string& param_name)
{
  try
  {
    static const char* names[] = {"temperature",
                                  "t2m",
                                  "t",
                                  "precipitation",
                                  "precipitation1h",
                                  "rr1h",
                                  "radarprecipitation1h",
                                  "precipitationtype",
                                  "rtype",
                                  "precipitationform",
                                  "rform",
                                  "precipitationprobability",
                                  "pop",
                                  "totalcloudcover",
                                  "cloudiness",
                                  "n",
                                  "relativehumidity",
                                  "windspeed",
                                  "windspeedms",
                                  "wspd",
                                  "ff",
                                  "winddirection",
                                  "dd",
                                  "wdir",
                                  "thunder",
                                  "probabilitythunderstorm",
                                  "pot",
                                  "roadtemperature",
                                  "troad",
                                  "roadcondition",
                                  "wroad",
                                  "waveheight",
                                  "wavedirection",
                                  "relativehumidity",
                                  "rh",
                                  "forestfirewarning",
                                  "forestfireindex",
                                  "mpi",
                                  "evaporation",
                                  "evap",
                                  "dewpoint",
                                  "tdew",
                                  "windgust",
                                  "gustspeed",
                                  "gust",
                                  "fogintensity",
                                  "fog",
                                  "maximumwind",
                                  "hourlymaximumwindspeed",
                                  "wmax",
                                  ""};

    static const char* parameters[] = {
        "Temperature",              // "Temperature"
        "Temperature",              // "t2m"
        "Temperature",              // "t"
        "Precipitation1h",          // "Precipitation"
        "Precipitation1h",          // "Precipitation1h"
        "Precipitation1h",          // "rr1h"
        "RadarPrecipitation1h",     // "radarprecipitation1h"
        "PrecipitationType",        // "PrecipitationType"
        "PrecipitationType",        // "rtype"
        "PrecipitationForm",        // "PrecipitationForm"
        "PrecipitationForm",        // "rform"
        "PoP",                      // "PrecipitationProbability"
        "PoP",                      // "pop"
        "TotalCloudCover",          // "TotalCloudCover"
        "TotalCloudCover",          // "Cloudiness"
        "TotalCloudCover",          // "n"
        "Humidity",                 // "RelativeHumidity"
        "WindSpeedMS",              // "WindSpeed"
        "WindSpeedMS",              // "WindSpeedMS"
        "WindSpeedMS",              // "wspd"
        "WindSpeedMS",              // "ff"
        "WindDirection",            // "WindDirection"
        "WindDirection",            // "dd"
        "WindDirection",            // "wdir"
        "ProbabilityThunderstorm",  // "Thunder"
        "ProbabilityThunderstorm",  // "ProbabilityThunderstorm"
        "ProbabilityThunderstorm",  // "pot"
        "RoadTemperature",          // "RoadTemperature"
        "RoadTemperature",          // "troad"
        "RoadCondition",            // "RoadCondition"
        "RoadCondition",            // "wroad"
        "SigWaveHeight",            // "WaveHeight"
        "WaveDirection",            // "WaveDirection"
        "RelativeHumidity",         // "RelativeHumidity"
        "RelativeHumidity",         // "rh"
        "ForestFireWarning",        // "ForestFireWarning"
        "ForestFireWarning",        // "ForestFireIndex"
        "ForestFireWarning",        // "mpi"
        "Evaporation",              // "Evaporation"
        "Evaporation",              // "evap"
        "DewPoint",                 // "DewPoint"
        "DewPoint",                 // "tdew"
        "WindGust",                 // "WindGust"
        "WindGust",                 // "GustSpeed"
        "WindGust",                 // "gust"
        "FogIntensity",             // "FogIntensity"
        "FogIntensity",             // "fog"
        "MaximumWind",              // "MaximumWind"
        "HourlyMaximumWindSpeed",   // "HourlyMaximumWindSpeed"
        "MaximumWind"               // "wmax"
    };

    for (unsigned int i = 0; names[i][0] != 0; i++)
    {
      if (param_name == names[i])
        return parameters[i];
    }

    return param_name;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the given parameter name and functions
 */
// ----------------------------------------------------------------------

std::string ParameterFactory::parse_parameter_functions(
    const std::string& theParameterRequest,
    std::string& theOriginalName,
    std::string& theParameterNameAlias,
    ParameterFunction& theInnerParameterFunction,
    ParameterFunction& theOuterParameterFunction) const
{
  try
  {
    std::string paramreq = theParameterRequest;
    std::string date_formatting_string;

    // extract alias name for the parameter
    size_t alias_name_pos(paramreq.find("as "));
    if (alias_name_pos != std::string::npos)
    {
      theParameterNameAlias = paramreq.substr(alias_name_pos + 3);
      paramreq = paramreq.substr(0, alias_name_pos);
      Fmi::trim(theParameterNameAlias);
      Fmi::trim(paramreq);
    }

    // special handling for date formatting, for example date(%Y-...)
    if (paramreq.find("date(") != std::string::npos)
    {
      size_t startIndex = paramreq.find("date(") + 4;
      std::string restOfTheParamReq = paramreq.substr(startIndex);

      if (restOfTheParamReq.find(")") == std::string::npos)
        throw Fmi::Exception(BCP, "Errorneous parameter request '" + theParameterRequest + "'!");

      size_t sizeOfTimeFormatString = restOfTheParamReq.find(")") + 1;
      restOfTheParamReq = restOfTheParamReq.substr(sizeOfTimeFormatString);
      date_formatting_string = paramreq.substr(startIndex, sizeOfTimeFormatString);
      paramreq.erase(startIndex, sizeOfTimeFormatString);
    }

    // If there is no timeseries functions involved, don't continue parsing
    if (paramreq.find("(") == std::string::npos)
    {
      theOriginalName = paramreq + date_formatting_string;
      Fmi::ascii_tolower(paramreq);

      return paramreq + date_formatting_string;
    }

    std::list<std::string> parts;
    std::string::size_type pos1 = 0;
    while (pos1 < paramreq.size())
    {
      std::string::size_type pos2 = pos1;
      for (; pos2 < paramreq.size(); ++pos2)
        if (paramreq[pos2] == '(' || paramreq[pos2] == ')')
          break;
      if (pos2 - pos1 > 0)
        parts.emplace_back(paramreq.substr(pos1, pos2 - pos1));

      pos1 = pos2 + 1;
    }

    if (parts.size() == 0 || parts.size() > 3)
      throw Fmi::Exception(BCP, "Errorneous parameter request '" + theParameterRequest + "'!");

    std::string paramname = parts.back();

    unsigned int aggregation_interval_behind = std::numeric_limits<unsigned int>::max();
    unsigned int aggregation_interval_ahead = std::numeric_limits<unsigned int>::max();
    std::string intervalSeparator(":");
    if (paramname.find("/") != std::string::npos)
      intervalSeparator = "/";
    else if (paramname.find(";") != std::string::npos)
      intervalSeparator = ";";
    else if (paramname.find(":") != std::string::npos)
      intervalSeparator = ":";

    if (paramname.find(intervalSeparator) != std::string::npos)
    {
      std::string aggregation_interval_string_behind =
          paramname.substr(paramname.find(intervalSeparator) + 1);
      std::string aggregation_interval_string_ahead = "0";
      paramname = paramname.substr(0, paramname.find(intervalSeparator));

      int agg_interval_behind = 0;
      int agg_interval_ahead = 0;
      // check if second aggregation interval is defined
      if (aggregation_interval_string_behind.find(intervalSeparator) != std::string::npos)
      {
        aggregation_interval_string_ahead = aggregation_interval_string_behind.substr(
            aggregation_interval_string_behind.find(intervalSeparator) + 1);
        aggregation_interval_string_behind = aggregation_interval_string_behind.substr(
            0, aggregation_interval_string_behind.find(intervalSeparator));
        agg_interval_ahead = duration_string_to_minutes(aggregation_interval_string_ahead);
        aggregation_interval_ahead = boost::numeric_cast<unsigned int>(agg_interval_ahead);
      }

      agg_interval_behind = duration_string_to_minutes(aggregation_interval_string_behind);

      if (agg_interval_behind < 0 || agg_interval_ahead < 0)
      {
        throw Fmi::Exception(BCP,
                             "The 'interval' option for '" + paramname + "' must be positive!");
      }
      aggregation_interval_behind = boost::numeric_cast<unsigned int>(agg_interval_behind);
    }

    parts.pop_back();
    const std::string functionname1 = (parts.empty() ? "" : parts.front());
    if (!parts.empty())
      parts.pop_front();
    const std::string functionname2 = (parts.empty() ? "" : parts.front());
    if (!parts.empty())
      parts.pop_front();

    if (functionname1.size() > 0 && functionname2.size() > 0)
    {
      // inner and outer functions exist
      auto f_name = extract_function(functionname2,
                                     theInnerParameterFunction.itsLowerLimit,
                                     theInnerParameterFunction.itsUpperLimit);

      theInnerParameterFunction.itsFunctionId = parse_function(f_name);
      theInnerParameterFunction.itsFunctionType =
          (f_name.substr(f_name.size() - 2).compare("_t") == 0 ? FunctionType::TimeFunction
                                                               : FunctionType::AreaFunction);

      theInnerParameterFunction.itsNaNFunction = (f_name.substr(0, 3).compare("nan") == 0);
      // Nearest && Interpolate functions always accepts NaNs in time series
      if (theInnerParameterFunction.itsFunctionId == FunctionId::Nearest ||
          theInnerParameterFunction.itsFunctionId == FunctionId::Interpolate)
        theInnerParameterFunction.itsNaNFunction = true;
      if (theInnerParameterFunction.itsFunctionType == FunctionType::TimeFunction)
      {
        theInnerParameterFunction.itsAggregationIntervalBehind = aggregation_interval_behind;
        theInnerParameterFunction.itsAggregationIntervalAhead = aggregation_interval_ahead;
      }

      f_name = (extract_function(functionname1,
                                 theOuterParameterFunction.itsLowerLimit,
                                 theOuterParameterFunction.itsUpperLimit));

      theOuterParameterFunction.itsFunctionId = parse_function(f_name);
      theOuterParameterFunction.itsFunctionType =
          (f_name.substr(f_name.size() - 2).compare("_t") == 0 ? FunctionType::TimeFunction
                                                               : FunctionType::AreaFunction);

      theOuterParameterFunction.itsNaNFunction = (f_name.substr(0, 3).compare("nan") == 0);

      if (theOuterParameterFunction.itsFunctionType == FunctionType::TimeFunction)
      {
        theOuterParameterFunction.itsAggregationIntervalBehind = aggregation_interval_behind;
        theOuterParameterFunction.itsAggregationIntervalAhead = aggregation_interval_ahead;
      }
    }
    else if (functionname1.size() > 0)
    {
      // only inner function exists,
      auto f_name = extract_function(functionname1,
                                     theInnerParameterFunction.itsLowerLimit,
                                     theInnerParameterFunction.itsUpperLimit);
      theInnerParameterFunction.itsFunctionId = parse_function(f_name);
      theInnerParameterFunction.itsFunctionType =
          (f_name.substr(f_name.size() - 2).compare("_t") == 0 ? FunctionType::TimeFunction
                                                               : FunctionType::AreaFunction);
      theInnerParameterFunction.itsNaNFunction = (f_name.substr(0, 3).compare("nan") == 0);
      // Nearest && Interpolate functions always accepts NaNs in time series
      if (theInnerParameterFunction.itsFunctionId == FunctionId::Nearest ||
          theInnerParameterFunction.itsFunctionId == FunctionId::Interpolate)
        theInnerParameterFunction.itsNaNFunction = true;

      if (theInnerParameterFunction.itsFunctionType == FunctionType::TimeFunction)
      {
        theInnerParameterFunction.itsAggregationIntervalBehind = aggregation_interval_behind;
        theInnerParameterFunction.itsAggregationIntervalAhead = aggregation_interval_ahead;
      }
    }

    if (theOuterParameterFunction.itsFunctionType == theInnerParameterFunction.itsFunctionType &&
        theOuterParameterFunction.itsFunctionType != FunctionType::NullFunctionType)
    {
      // remove outer function
      theOuterParameterFunction = ParameterFunction();
    }

    // We assume ASCII chars only in parameter names
    theOriginalName = paramname + date_formatting_string;
    Fmi::ascii_tolower(paramname);
    return paramname + date_formatting_string;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

ParameterAndFunctions ParameterFactory::parseNameAndFunctions(
    const std::string& name, bool ignoreBadParameter /* = false*/) const
{
  try
  {
    auto tmpname = Fmi::trim_copy(name);

    ParameterFunction innerFunction;
    ParameterFunction outerFunction;

    size_t parenhesis_start = std::count(tmpname.begin(), tmpname.end(), '(');
    size_t parenhesis_end = std::count(tmpname.begin(), tmpname.end(), ')');

    if (parenhesis_start != parenhesis_end)
      throw Fmi::Exception(BCP, "Wrong number of parenthesis: " + tmpname);

    std::string sensor_no;
    std::string sensor_parameter;

    std::string innermost_item = tmpname;
    // If sensor info exists it is inside the innermost parenthesis
    while (innermost_item.find_first_of("(") != innermost_item.find_last_of("("))
    {
      size_t count = innermost_item.find_last_of(")") - innermost_item.find_first_of("(") - 1;
      innermost_item = innermost_item.substr(innermost_item.find_first_of("(") + 1, count);
    }

    if (!boost::algorithm::istarts_with(name, "date(") &&
        innermost_item.find("(") != std::string::npos)
    {
      Fmi::trim(innermost_item);
      std::string innermost_name = innermost_item.substr(0, innermost_item.find("("));
      if (innermost_item.find("[") != std::string::npos)
      {
        // Remove [..., for example percentage_t[0:60](TotalCloudCover)
        innermost_name = innermost_name.substr(0, innermost_item.find("["));
      }
      bool sensor_parameter_exists = false;
      // If the name before innermost parenthesis is not a function it must be a parameter

      if (get_function_index(innermost_name) < 0)
      {
        // Sensor info
        auto len = innermost_item.find(")") - innermost_item.find("(") + 1;
        auto sensor_info = innermost_item.substr(innermost_item.find("("), len);
        if (sensor_info.find(":") != sensor_info.rfind(":"))
        {
          auto len = sensor_info.rfind(")") - sensor_info.rfind(":") - 1;
          sensor_parameter = sensor_info.substr(sensor_info.rfind(":") + 1, len);
          len = sensor_info.rfind(":") - sensor_info.find(":") - 1;
          sensor_no = sensor_info.substr(sensor_info.find(":") + 1, len);
          sensor_parameter_exists = true;
        }
        else if (sensor_info.find(":") != std::string::npos)
        {
          size_t len = sensor_info.rfind(")") - sensor_info.find(":") - 1;
          sensor_no = sensor_info.substr(sensor_info.find(":") + 1, len);
        }
        Fmi::trim(sensor_parameter);
        Fmi::trim(sensor_no);

        if (sensor_no.empty())
          throw Fmi::Exception(BCP, "Sensor number can not be empty!");
        if (sensor_parameter_exists &&
            (sensor_parameter.empty() ||
             (sensor_parameter != "qc" && sensor_parameter != "longitude" &&
              sensor_parameter != "latitude")))
          throw Fmi::Exception(BCP,
                               "Sensor parameter must be of the following: qc,longitide,latitude!");

        boost::algorithm::replace_first(tmpname, sensor_info, "");
      }
    }

    std::string paramnameAlias = tmpname;
    std::string originalParamName = tmpname;

    std::string paramname = parse_parameter_name(parse_parameter_functions(
        tmpname, originalParamName, paramnameAlias, innerFunction, outerFunction));
    Parameter parameter = parse(paramname, ignoreBadParameter);

    parameter.setAlias(paramnameAlias);
    parameter.setOriginalName(originalParamName);

    if (boost::algorithm::starts_with(paramname, "qc_"))
      sensor_parameter = "qc";

    if (!sensor_no.empty())
      parameter.setSensorNumber(Fmi::stoi(sensor_no));
    if (!sensor_parameter.empty())
      parameter.setSensorParameter(sensor_parameter);

    return ParameterAndFunctions(parameter, ParameterFunctions(innerFunction, outerFunction));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}  // namespace Spine

// ----------------------------------------------------------------------
/*!
 * \brief Parse the given parameter name
 */
// ----------------------------------------------------------------------

Parameter ParameterFactory::parse(const std::string& paramname,
                                  bool ignoreBadParameter /* = false*/) const
{
  try
  {
    if (paramname.empty())
      throw Fmi::Exception(BCP, "Empty parameters are not allowed!");

    // Metaparameters are required to have a FmiParameterName too
    FmiParameterName number = FmiParameterName(converter.ToEnum(paramname));

    if (number == kFmiBadParameter && Fmi::looks_signed_int(paramname))
      number = FmiParameterName(Fmi::stol(paramname));

    Parameter::Type type = Parameter::Type::Data;

    if (Parameters::IsLandscaped(number))
      type = Parameter::Type::Landscaped;
    else if (Parameters::IsDataIndependent(number))
      type = Parameter::Type::DataIndependent;
    else if (Parameters::IsDataDerived(number))
      type = Parameter::Type::DataDerived;
    else if (paramname.substr(0, 5) == "date(" && paramname[paramname.size() - 1] == ')')
      type = Parameter::Type::DataIndependent;
    else if (boost::algorithm::iends_with(paramname, ".raw"))
      number = FmiParameterName(converter.ToEnum(paramname.substr(0, paramname.size() - 4)));

    if (number == kFmiBadParameter && !ignoreBadParameter)
      throw Fmi::Exception(BCP, "Unknown parameter '" + paramname + "'");

    return Parameter(paramname, type, number);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace Spine
}  // namespace SmartMet
