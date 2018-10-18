// ======================================================================
/*!
 * \brief generate parameter descriptor from parameter name
 */
// ======================================================================

#include "ParameterFactory.h"
#include "Convenience.h"
#include "Exception.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <macgyver/StringConversion.h>
#include <stdexcept>

using namespace std;

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

FunctionId ParameterFactory::parse_function(const string& theFunction) const
{
  try
  {
    static const char* names[] = {
        "mean_a",          "mean_t",          "nanmean_a",  "nanmean_t",    "max_a",
        "max_t",           "nanmax_a",        "nanmax_t",   "min_a",        "min_t",
        "nanmin_a",        "nanmin_t",        "median_a",   "median_t",     "nanmedian_a",
        "nanmedian_t",     "sum_a",           "sum_t",      "nansum_a",     "nansum_t",
        "integ_a",         "integ_t",         "naninteg_a", "naninteg_t",   "sdev_a",
        "sdev_t",          "nansdev_a",       "nansdev_t",  "percentage_a", "percentage_t",
        "nanpercentage_a", "nanpercentage_t", "count_a",    "count_t",      "nancount_a",
        "nancount_t",      "change_a",        "change_t",   "nanchange_a",  "nanchange_t",
        "trend_a",         "trend_t",         "nantrend_a", "nantrend_t",   ""};

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
                                     FunctionId::Trend};

    std::string func_name(theFunction);

    // if ending is missing, add area aggregation ending
    if (func_name.find("_a") == std::string::npos && func_name.find("_t") == std::string::npos)
      func_name += "_a";

    for (unsigned int i = 0; strlen(names[i]) > 0; i++)
    {
      if (names[i] == func_name)
        return functions[i];
    }

    throw Spine::Exception(BCP, "Unrecognized function name '" + theFunction + "'!");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
string ParameterFactory::extract_limits(const string& theString) const
{
  try
  {
    string::size_type pos1 = theString.find('[');
    if (pos1 == string::npos)
      return "";

    string::size_type pos2 = theString.find(']', pos1);
    if (pos2 == string::npos && pos2 != theString.size() - 1)
      throw Spine::Exception(BCP, "Invalid function modifier in '" + theString + "'!");

    return theString.substr(pos1 + 1, pos2 - pos1 - 1);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

string ParameterFactory::extract_function(const string& theString,
                                          double& theLowerLimit,
                                          double& theUpperLimit) const
{
  try
  {
    string::size_type pos = theString.find('[');

    theLowerLimit = -std::numeric_limits<double>::max();
    theUpperLimit = std::numeric_limits<double>::max();

    if (pos == string::npos)
    {
      return theString;
    }
    else
    {
      string function_name(theString.substr(0, pos));

      string limits(extract_limits(theString));

      if (!limits.empty())
      {
        const string::size_type pos = limits.find(':');

        if (pos == string::npos)
          throw Spine::Exception(BCP, "Unrecognized modifier format '" + limits + "'!");

        string lo = limits.substr(0, pos);
        string hi = limits.substr(pos + 1);
        boost::algorithm::trim(lo);
        boost::algorithm::trim(hi);

        if (lo.empty() && hi.empty())
          throw Spine::Exception(BCP, "Both lower and upper limit are missing from the modifier!");

        if (!lo.empty())
          theLowerLimit = Fmi::stod(lo);
        if (!hi.empty())
          theUpperLimit = Fmi::stod(hi);
      }

      return function_name;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

string parse_parameter_name(const string& param_name)
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
        "Precipitation1h",          // "radarprecipitation1h"
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

    for (unsigned int i = 0; strlen(names[i]) > 0; i++)
    {
      if (param_name == names[i])
        return parameters[i];
    }

    return param_name;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the given parameter name and functions
 */
// ----------------------------------------------------------------------
std::string ParameterFactory::parse_parameter_functions(
    const std::string& theParameterRequest,
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
      boost::algorithm::trim(theParameterNameAlias);
      boost::algorithm::trim(paramreq);
    }

    // special handling for date formatting, for example date(%Y-...)
    if (paramreq.find("date(") != std::string::npos)
    {
      size_t startIndex = paramreq.find("date(") + 4;
      std::string restOfTheParamReq = paramreq.substr(startIndex);

      if (restOfTheParamReq.find(")") == std::string::npos)
        throw Spine::Exception(BCP, "Errorneous parameter request '" + theParameterRequest + "'!");

      size_t sizeOfTimeFormatString = restOfTheParamReq.find(")") + 1;
      restOfTheParamReq = restOfTheParamReq.substr(sizeOfTimeFormatString);
      date_formatting_string = paramreq.substr(startIndex, sizeOfTimeFormatString);
      paramreq.erase(startIndex, sizeOfTimeFormatString);
    }

    list<string> parts;
    string::size_type pos1 = 0;
    while (pos1 < paramreq.size())
    {
      string::size_type pos2 = pos1;
      for (; pos2 < paramreq.size(); ++pos2)
        if (paramreq[pos2] == '(' || paramreq[pos2] == ')')
          break;
      if (pos2 - pos1 > 0)
        parts.push_back(paramreq.substr(pos1, pos2 - pos1));

      pos1 = pos2 + 1;
    }

    if (parts.size() == 0 || parts.size() > 3)
      throw Spine::Exception(BCP, "Errorneous parameter request '" + theParameterRequest + "'!");

    string paramname = parts.back();

    unsigned int aggregation_interval_behind = std::numeric_limits<unsigned int>::max();
    unsigned int aggregation_interval_ahead = std::numeric_limits<unsigned int>::max();
    if (paramname.find(":") != string::npos)
    {
      std::string aggregation_interval_string_behind = paramname.substr(paramname.find(":") + 1);
      std::string aggregation_interval_string_ahead = "0";
      paramname = paramname.substr(0, paramname.find(":"));

      int agg_interval_behind = 0;
      int agg_interval_ahead = 0;
      // check if second aggregation interval is defined
      if (aggregation_interval_string_behind.find(":") != string::npos)
      {
        aggregation_interval_string_ahead = aggregation_interval_string_behind.substr(
            aggregation_interval_string_behind.find(":") + 1);
        aggregation_interval_string_behind = aggregation_interval_string_behind.substr(
            0, aggregation_interval_string_behind.find(":"));
        agg_interval_ahead = duration_string_to_minutes(aggregation_interval_string_ahead);
        aggregation_interval_ahead = boost::numeric_cast<unsigned int>(agg_interval_ahead);
      }

      agg_interval_behind = duration_string_to_minutes(aggregation_interval_string_behind);

      if (agg_interval_behind < 0 || agg_interval_ahead < 0)
      {
        throw Spine::Exception(BCP,
                               "The 'interval' option for '" + paramname + "' must be positive!");
      }
      aggregation_interval_behind = boost::numeric_cast<unsigned int>(agg_interval_behind);
    }

    parts.pop_back();
    const string functionname1 = (parts.empty() ? "" : parts.front());
    if (!parts.empty())
      parts.pop_front();
    const string functionname2 = (parts.empty() ? "" : parts.front());
    if (!parts.empty())
      parts.pop_front();

    if (functionname1.size() > 0 && functionname2.size() > 0)
    {
      // inner and outer functions exist
      string f_name = (extract_function(functionname2,
                                        theInnerParameterFunction.itsLowerLimit,
                                        theInnerParameterFunction.itsUpperLimit));
      theInnerParameterFunction.itsFunctionId = parse_function(f_name);
      theInnerParameterFunction.itsFunctionType =
          (f_name.substr(f_name.size() - 2).compare("_t") == 0 ? FunctionType::TimeFunction
                                                               : FunctionType::AreaFunction);

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
      string f_name = (extract_function(functionname1,
                                        theInnerParameterFunction.itsLowerLimit,
                                        theInnerParameterFunction.itsUpperLimit));
      theInnerParameterFunction.itsFunctionId = parse_function(f_name);
      theInnerParameterFunction.itsFunctionType =
          (f_name.substr(f_name.size() - 2).compare("_t") == 0 ? FunctionType::TimeFunction
                                                               : FunctionType::AreaFunction);
      theInnerParameterFunction.itsNaNFunction = (f_name.substr(0, 3).compare("nan") == 0);

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
    Fmi::ascii_tolower(paramname);
    return paramname + date_formatting_string;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ParameterAndFunctions ParameterFactory::parseNameAndFunctions(
    const std::string& name, bool ignoreBadParameter /* = false*/) const
{
  try
  {
    auto tmpname = boost::algorithm::trim_copy(name);

    ParameterFunction innerFunction;
    ParameterFunction outerFunction;

    std::string paramnameAlias = tmpname;

    std::string paramname = parse_parameter_name(
        parse_parameter_functions(tmpname, paramnameAlias, innerFunction, outerFunction));
    Parameter parameter = parse(paramname, ignoreBadParameter);

    parameter.setAlias(paramnameAlias);

    return ParameterAndFunctions(parameter, ParameterFunctions(innerFunction, outerFunction));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

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
      throw Spine::Exception(BCP, "Empty parameters are not allowed!");

    // Allow both WindChill and windchill
    auto p = boost::algorithm::to_lower_copy(paramname);

    auto name = paramname;
    Parameter::Type type = Parameter::Type::Data;

    // some metaparameters may have a matching number (WindUMS, WindVMS)
    FmiParameterName number = FmiParameterName(converter.ToEnum(p));

    if (p == "dewpoint" || p == "temperature" || p == "minimumtemperature06" ||
        p == "maximumtemperature06" || p == "minimumtemperature24h" ||
        p == "maximumtemperature24h" || p == "dailymeantemperature" || p == "temperaturef0" ||
        p == "temperaturef10" || p == "temperaturef25" || p == "temperaturef50" ||
        p == "temperaturef75" || p == "temperaturef90" || p == "temperaturef100")
    {
      type = Parameter::Type::Landscaped;
      if (number == kFmiBadParameter)
        if (!ignoreBadParameter)
          throw Spine::Exception(BCP, "Unknown parameter to be landscaped '" + paramname + "'!");
    }

    else if (p == "country" || p == "covertype" || p == "dark" || p == "daylength" || p == "dem" ||
             p == "direction" || p == "distance" || p == "elevation" || p == "epochtime" ||
             p == "feature" || p == "fmisid" || p == "geoid" || p == "hour" || p == "iso2" ||
             p == "isotime" || p == "level" || p == "localtime" || p == "localtz" || p == "lpnn" ||
             p == "model" || p == "modtime" || p == "mtime" || p == "mon" || p == "month" ||
             p == "moondown24h" || p == "moonphase" || p == "moonrise" || p == "moonrise2" ||
             p == "moonrise2today" || p == "moonrisetoday" || p == "moonset" || p == "moonset2" ||
             p == "moonset2today" || p == "moonsettoday" || p == "moonup24h" || p == "name" ||
             p == "noon" || p == "origintime" || p == "place" || p == "population" ||
             p == "region" || p == "rwsid" || p == "sensor_no" || p == "stationary" ||
             p == "station_elevation" || p == "stationlat" || p == "stationlatitude" ||
             p == "stationlon" || p == "stationlongitude" || p == "station_name" ||
             p == "stationname" || p == "sunazimuth" || p == "sundeclination" ||
             p == "sunelevation" || p == "sunrise" || p == "sunrisetoday" || p == "sunset" ||
             p == "sunsettoday" || p == "time" || p == "timestring" || p == "tz" ||
             p == "utctime" || p == "wday" || p == "weekday" || p == "wmo" || p == "xmltime" ||
             p == "timestring" || p == "nearlatitude" || p == "nearlongitude" ||
             p == "nearlatlon" || p == "nearlonlat" || p == "gridnorth")
    {
      type = Parameter::Type::DataIndependent;
    }
    else if (p == "windcompass8" || p == "windcompass16" || p == "windcompass32" ||
             p == "cloudiness8th" || p == "windchill" || p == "summersimmerindex" || p == "ssi" ||
             p == "feelslike" || p == "weather" || p == "weathersymbol" ||
             p == "apparenttemperature" || p == "snow1hlower" || p == "snow1hupper" ||
             p == "snow1h" || p == "smartsymbol" || p == "smartsymboltext" ||
             p == "weathernumber" || p == "windums" || p == "windvms" || p == "lat" ||
             p == "latitude" || p == "latlon" || p == "lon" || p == "longitude" || p == "lonlat" ||
             p == "data_source")
    {
      type = Parameter::Type::DataDerived;
    }

    // Allow date(...)
    else if (p.substr(0, 5) == "date(" && p[p.size() - 1] == ')')
    {
      type = Parameter::Type::DataIndependent;
    }
    else
    {
      if (!boost::algorithm::iends_with(p, ".raw"))
        number = FmiParameterName(converter.ToEnum(p));
      else
        number = FmiParameterName(converter.ToEnum(p.substr(0, p.size() - 4)));

      if (number == kFmiBadParameter)
      {
        // Prefer regex instead of a try..catch block
        if (boost::regex_match(p, boost::regex("^[(-|+)]?[0-9]+$")))
          number = FmiParameterName(Fmi::stol(p));
        else if (!ignoreBadParameter)
          throw Spine::Exception(BCP, "Unknown parameter '" + paramname + "'!");
      }
    }

    return Parameter(name, type, number);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
