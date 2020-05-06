#include "ParameterTools.h"
#include "ParameterKeywords.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Astronomy.h>
#include <macgyver/CharsetTools.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <set>

namespace SmartMet
{
namespace Spine
{
namespace
{
const std::set<std::string> location_parameters = {LEVEL_PARAM,
                                                   LATITUDE_PARAM,
                                                   LONGITUDE_PARAM,
                                                   LAT_PARAM,
                                                   LON_PARAM,
                                                   LATLON_PARAM,
                                                   LONLAT_PARAM,
                                                   GEOID_PARAM,
                                                   // PLACE_PARAM,
                                                   FEATURE_PARAM,
                                                   LOCALTZ_PARAM,
                                                   NAME_PARAM,
                                                   ISO2_PARAM,
                                                   REGION_PARAM,
                                                   COUNTRY_PARAM,
                                                   ELEVATION_PARAM,
                                                   POPULATION_PARAM,
                                                   STATION_ELEVATION_PARAM};

const std::map<std::string, Parameter::Type> special_parameter_map = {
    {"cloudiness8th", Parameter::Type::DataDerived},
    {COUNTRY_PARAM, Parameter::Type::DataIndependent},
    {DARK_PARAM, Parameter::Type::DataIndependent},
    {DIRECTION_PARAM, Parameter::Type::DataIndependent},
    {DISTANCE_PARAM, Parameter::Type::DataIndependent},
    {ELEVATION_PARAM, Parameter::Type::DataIndependent},
    {EPOCHTIME_PARAM, Parameter::Type::DataIndependent},
    {"feelslike", Parameter::Type::DataDerived},
    {FMISID_PARAM, Parameter::Type::DataIndependent},
    {GEOID_PARAM, Parameter::Type::DataIndependent},
    {HOUR_PARAM, Parameter::Type::DataIndependent},
    {ISO2_PARAM, Parameter::Type::DataIndependent},
    {ISOTIME_PARAM, Parameter::Type::DataIndependent},
    {LAT_PARAM, Parameter::Type::DataIndependent},
    {LATITUDE_PARAM, Parameter::Type::DataIndependent},
    {LATLON_PARAM, Parameter::Type::DataIndependent},
    //{ LEVEL_PARAM, Parameter::Type::DataIndependent },
    {LOCALTIME_PARAM, Parameter::Type::DataIndependent},
    {LOCALTZ_PARAM, Parameter::Type::DataIndependent},
    {LON_PARAM, Parameter::Type::DataIndependent},
    {LONGITUDE_PARAM, Parameter::Type::DataIndependent},
    {LONLAT_PARAM, Parameter::Type::DataIndependent},
    {LPNN_PARAM, Parameter::Type::DataIndependent},
    {MODEL_PARAM, Parameter::Type::DataIndependent},
    {"modtime", Parameter::Type::DataIndependent},
    {MON_PARAM, Parameter::Type::DataIndependent},
    {MONTH_PARAM, Parameter::Type::DataIndependent},
    {MOONPHASE_PARAM, Parameter::Type::DataIndependent},
    {NAME_PARAM, Parameter::Type::DataIndependent},
    {NOON_PARAM, Parameter::Type::DataIndependent},
    {ORIGINTIME_PARAM, Parameter::Type::DataIndependent},
    {PLACE_PARAM, Parameter::Type::DataIndependent},
    {REGION_PARAM, Parameter::Type::DataIndependent},
    //{ RWSID_PARAM, Parameter::Type::DataIndependent },
    //{ SENSOR_NO_PARAM, Parameter::Type::DataIndependent },
    {"smartsymbol", Parameter::Type::DataDerived},
    //{ "stationary", Parameter::Type::DataIndependent },
    {STATION_ELEVATION_PARAM, Parameter::Type::DataIndependent},
    {STATIONLAT_PARAM, Parameter::Type::DataIndependent},
    {STATIONLATITUDE_PARAM, Parameter::Type::DataIndependent},
    {STATIONLON_PARAM, Parameter::Type::DataIndependent},
    {STATIONLONGITUDE_PARAM, Parameter::Type::DataIndependent},
    {STATION_NAME_PARAM, Parameter::Type::DataIndependent},
    {STATIONNAME_PARAM, Parameter::Type::DataIndependent},
    {SUNAZIMUTH_PARAM, Parameter::Type::DataIndependent},
    {SUNDECLINATION_PARAM, Parameter::Type::DataIndependent},
    {SUNELEVATION_PARAM, Parameter::Type::DataIndependent},
    {SUNRISE_PARAM, Parameter::Type::DataIndependent},
    {SUNRISETODAY_PARAM, Parameter::Type::DataIndependent},
    {SUNSET_PARAM, Parameter::Type::DataIndependent},
    {SUNSETTODAY_PARAM, Parameter::Type::DataIndependent},
    {TIME_PARAM, Parameter::Type::DataIndependent},
    {TIMESTRING_PARAM, Parameter::Type::DataIndependent},
    {TZ_PARAM, Parameter::Type::DataIndependent},
    {UTCTIME_PARAM, Parameter::Type::DataIndependent},
    {"weather", Parameter::Type::DataDerived},
    {"windchill", Parameter::Type::DataDerived},
    {"windcompass16", Parameter::Type::DataDerived},
    {"windcompass32", Parameter::Type::DataDerived},
    {"windcompass8", Parameter::Type::DataDerived},
    {WMO_PARAM, Parameter::Type::DataIndependent},
    {XMLTIME_PARAM, Parameter::Type::DataIndependent}};

// ----------------------------------------------------------------------
/*!
 * \brief Time formatter
 */
// ----------------------------------------------------------------------

std::string format_date(const boost::local_time::local_date_time& ldt,
                        const std::locale& llocale,
                        const std::string& fmt)
{
  try
  {
    typedef boost::date_time::time_facet<boost::local_time::local_date_time, char> tfacet;
    std::ostringstream os;
    os.imbue(std::locale(llocale, new tfacet(fmt.c_str())));
    os << ldt;
    return Fmi::latin1_to_utf8(os.str());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // anonymous namespace

bool special(const Parameter& theParam)
{
  try
  {
    switch (theParam.type())
    {
      case Spine::Parameter::Type::Data:
      case Spine::Parameter::Type::Landscaped:
        return false;
      case Spine::Parameter::Type::DataDerived:
      case Spine::Parameter::Type::DataIndependent:
        return true;
    }
    // ** NOT REACHED **
    return true;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the parameter if of aggregatable type
 */
// ----------------------------------------------------------------------

bool parameter_is_arithmetic(const Spine::Parameter& theParameter)
{
  try
  {
    switch (theParameter.type())
    {
      case Spine::Parameter::Type::Data:
      case Spine::Parameter::Type::Landscaped:
      case Spine::Parameter::Type::DataDerived:
        return true;
      case Spine::Parameter::Type::DataIndependent:
        return false;
    }
    // NOT REACHED //
    return false;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the given parameters depend on location only
 */
// ----------------------------------------------------------------------

bool is_plain_location_query(const Spine::OptionParsers::ParameterList& theParams)
{
  try
  {
    for (const auto& param : theParams)
    {
      const auto& name = param.name();
      if (not is_location_parameter(name))
      {
        return false;
      }
    }
    return true;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

bool is_location_parameter(const std::string& paramname)
{
  return location_parameters.count(paramname) > 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

std::string location_parameter(const Spine::LocationPtr loc,
                               const std::string paramName,
                               const Spine::ValueFormatter& valueformatter,
                               const std::string& timezone,
                               int precision)
{
  try
  {
    if (!loc)
      return valueformatter.missing();
    if (paramName == NAME_PARAM)
      return loc->name;
    if (paramName == STATIONNAME_PARAM)
      return loc->name;
    if (paramName == ISO2_PARAM)
      return loc->iso2;
    if (paramName == GEOID_PARAM)
    {
      if (loc->geoid == 0)
        return valueformatter.missing();

      return Fmi::to_string(loc->geoid);
    }
    if (paramName == REGION_PARAM)
    {
      if (loc->area.empty())
      {
        if (loc->name.empty())
        {
          // No area (administrative region) nor name known.
          return valueformatter.missing();
        }
        // Place name known, administrative region unknown.
        return loc->name;
      }
      // Administrative region known.
      return loc->area;
    }
    if (paramName == COUNTRY_PARAM)
      return loc->country;
    if (paramName == FEATURE_PARAM)
      return loc->feature;
    if (paramName == LOCALTZ_PARAM)
      return loc->timezone;
    if (paramName == TZ_PARAM)
      return (timezone == "localtime" ? loc->timezone : timezone);
    if (paramName == LATITUDE_PARAM || paramName == LAT_PARAM)
      return valueformatter.format(loc->latitude, precision);
    if (paramName == LONGITUDE_PARAM || paramName == LON_PARAM)
      return valueformatter.format(loc->longitude, precision);
    if (paramName == LATLON_PARAM)
      return (valueformatter.format(loc->latitude, precision) + ", " +
              valueformatter.format(loc->longitude, precision));
    if (paramName == LONLAT_PARAM)
      return (valueformatter.format(loc->longitude, precision) + ", " +
              valueformatter.format(loc->latitude, precision));
    if (paramName == POPULATION_PARAM)
      return Fmi::to_string(loc->population);
    if (paramName == ELEVATION_PARAM || paramName == STATION_ELEVATION_PARAM)
      return valueformatter.format(loc->elevation, precision);

    throw Spine::Exception(BCP, "Unknown location parameter: '" + paramName + "'");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool is_time_parameter(std::string paramname)
{
  Fmi::ascii_tolower(paramname);
  try
  {
    return (
        paramname == TIME_PARAM || paramname == ISOTIME_PARAM || paramname == XMLTIME_PARAM ||
        paramname == ORIGINTIME_PARAM || paramname == LOCALTIME_PARAM ||
        paramname == UTCTIME_PARAM || paramname == EPOCHTIME_PARAM ||
        paramname == SUNELEVATION_PARAM || paramname == SUNDECLINATION_PARAM ||
        paramname == SUNAZIMUTH_PARAM || paramname == DARK_PARAM || paramname == MOONPHASE_PARAM ||
        paramname == MOONRISE_PARAM || paramname == MOONRISE2_PARAM || paramname == MOONSET_PARAM ||
        paramname == MOONSET2_PARAM || paramname == MOONRISETODAY_PARAM ||
        paramname == MOONRISE2TODAY_PARAM || paramname == MOONSETTODAY_PARAM ||
        paramname == MOONSET2TODAY_PARAM || paramname == MOONUP24H_PARAM ||
        paramname == MOONDOWN24H_PARAM || paramname == SUNRISE_PARAM || paramname == SUNSET_PARAM ||
        paramname == NOON_PARAM || paramname == SUNRISETODAY_PARAM ||
        paramname == SUNSETTODAY_PARAM || paramname == DAYLENGTH_PARAM ||
        paramname == TIMESTRING_PARAM || paramname == WDAY_PARAM || paramname == WEEKDAY_PARAM ||
        paramname == MON_PARAM || paramname == MONTH_PARAM || paramname == HOUR_PARAM ||
        paramname == TZ_PARAM ||
        (paramname.substr(0, 5) == "date(" && paramname[paramname.size() - 1] == ')'));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

Spine::TimeSeries::Value time_parameter(const std::string paramname,
                                        const boost::local_time::local_date_time& ldt,
                                        const boost::posix_time::ptime now,
                                        const Spine::Location& loc,
                                        const std::string& timezone,
                                        const Fmi::TimeZones& timezones,
                                        const std::locale& outlocale,
                                        const Fmi::TimeFormatter& timeformatter,
                                        const std::string& timestring)
{
  using boost::local_time::local_date_time;

  try
  {
    Spine::TimeSeries::Value ret = Spine::TimeSeries::None();

    if (paramname == TIME_PARAM)
    {
      boost::local_time::time_zone_ptr tz = timezones.time_zone_from_string(timezone);
      ret = timeformatter.format(local_date_time(ldt.utc_time(), tz));
    }

    if (paramname == ORIGINTIME_PARAM)
    {
      boost::local_time::time_zone_ptr tz = timezones.time_zone_from_string(timezone);
      local_date_time ldt_now(now, tz);
      ret = timeformatter.format(ldt_now);
    }

    if (paramname == ISOTIME_PARAM)
      ret = Fmi::to_iso_string(ldt.local_time());

    if (paramname == XMLTIME_PARAM)
      ret = Fmi::to_iso_extended_string(ldt.local_time());

    if (paramname == LOCALTIME_PARAM)
    {
      boost::local_time::time_zone_ptr localtz = timezones.time_zone_from_string(loc.timezone);

      boost::posix_time::ptime utc = ldt.utc_time();
      boost::local_time::local_date_time localt(utc, localtz);
      ret = timeformatter.format(localt);
    }

    if (paramname == UTCTIME_PARAM)
      ret = timeformatter.format(ldt.utc_time());

    if (paramname == EPOCHTIME_PARAM)
    {
      boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
      boost::posix_time::time_duration diff = ldt.utc_time() - time_t_epoch;
      ret = Fmi::to_string(diff.total_seconds());
    }

    if (paramname == ORIGINTIME_PARAM)
      ret = timeformatter.format(now);

    if (paramname == TZ_PARAM)
      ret = timezone;

    if (paramname == SUNELEVATION_PARAM)
    {
      Fmi::Astronomy::solar_position_t sp =
          Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      ret = sp.elevation;
    }

    if (paramname == SUNDECLINATION_PARAM)
    {
      Fmi::Astronomy::solar_position_t sp =
          Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      ret = sp.declination;
    }

    if (paramname == SUNAZIMUTH_PARAM)
    {
      Fmi::Astronomy::solar_position_t sp =
          Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      ret = sp.azimuth;
    }

    if (paramname == DARK_PARAM)
    {
      Fmi::Astronomy::solar_position_t sp =
          Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_string(sp.dark());
    }

    if (paramname == MOONPHASE_PARAM)
      ret = Fmi::Astronomy::moonphase(ldt.utc_time());

    if (paramname == MOONRISE_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      if (lt.moonrise_today())
        ret = Fmi::to_iso_string(lt.moonrise.local_time());
    }
    if (paramname == MOONRISE2_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

      if (lt.moonrise2_today())
        ret = Fmi::to_iso_string(lt.moonrise2.local_time());
    }
    if (paramname == MOONSET_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      if (lt.moonset_today())
        ret = Fmi::to_iso_string(lt.moonset.local_time());
    }
    if (paramname == MOONSET2_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

      if (lt.moonset2_today())
        ret = Fmi::to_iso_string(lt.moonset2.local_time());
    }

    if (paramname == MOONRISETODAY_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

      ret = Fmi::to_string(lt.moonrise_today());
    }
    if (paramname == MOONRISE2TODAY_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_string(lt.moonrise2_today());
    }
    if (paramname == MOONSETTODAY_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_string(lt.moonset_today());
    }
    if (paramname == MOONSET2TODAY_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_string(lt.moonset2_today());
    }
    if (paramname == MOONUP24H_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_string(lt.above_horizont_24h());
    }
    if (paramname == MOONDOWN24H_PARAM)
    {
      Fmi::Astronomy::lunar_time_t lt =
          Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_string(!lt.moonrise_today() && !lt.moonset_today() && !lt.above_horizont_24h());
    }
    if (paramname == SUNRISE_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_iso_string(st.sunrise.local_time());
    }
    if (paramname == SUNSET_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_iso_string(st.sunset.local_time());
    }
    if (paramname == NOON_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_iso_string(st.noon.local_time());
    }
    if (paramname == SUNRISETODAY_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_string(st.sunrise_today());
    }
    if (paramname == SUNSETTODAY_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      ret = Fmi::to_string(st.sunset_today());
    }
    if (paramname == DAYLENGTH_PARAM)
    {
      Fmi::Astronomy::solar_time_t st =
          Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      auto seconds = st.daylength().total_seconds();
      int minutes = boost::numeric_cast<int>(round(static_cast<double>(seconds) / 60.0));
      ret = minutes;
    }
    if (paramname == TIMESTRING_PARAM)
      ret = format_date(ldt, outlocale, timestring.c_str());

    if (paramname == WDAY_PARAM)
      ret = format_date(ldt, outlocale, "%a");

    if (paramname == WEEKDAY_PARAM)
      ret = format_date(ldt, outlocale, "%A");

    if (paramname == MON_PARAM)
      ret = format_date(ldt, outlocale, "%b");

    if (paramname == MONTH_PARAM)
    {
      ret = format_date(ldt, outlocale, "%B");
    }

    if (paramname == HOUR_PARAM)
      ret = Fmi::to_string(ldt.local_time().time_of_day().hours());
    if (paramname.substr(0, 5) == "date(" && paramname[paramname.size() - 1] == ')')
      ret = format_date(ldt, outlocale, paramname.substr(5, paramname.size() - 6));

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool is_special_parameter(const std::string& name)
{
  std::string p = boost::algorithm::to_lower_copy(name, std::locale::classic());
  return special_parameter_map.count(p) > 0;
}

Spine::Parameter makeParameter(const std::string& name)
{
  try
  {
    if (name.empty())
      throw Spine::Exception(BCP, "Empty parameters are not allowed");

    std::string p = boost::algorithm::to_lower_copy(name, std::locale::classic());
    Spine::Parameter::Type type;

    auto it = special_parameter_map.find(p);
    if (it != special_parameter_map.end())
    {
      type = it->second;
    }
    else if (boost::algorithm::ends_with(p, "data_source"))
    {
      type = Parameter::Type::DataDerived;
    }
    else
    {
      type = Parameter::Type::Data;
    }

    return Spine::Parameter(name, type);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
