#include "Value.h"
#include "ConfigBase.h"
#include <boost/algorithm/string.hpp>
#include <boost/current_function.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/spirit/include/qi.hpp>
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <macgyver/TypeName.h>
#include <algorithm>
#include <stdexcept>

using Fmi::demangle_cpp_type_name;

namespace
{
using SmartMet::Spine::Value;

template <typename NumType>
NumType check_limits_impl(NumType arg,
                          const boost::optional<Value>& lower_limit,
                          const boost::optional<Value>& upper_limit,
                          NumType (Value::*getter)() const)
{
  try
  {
    if ((not lower_limit or (arg >= ((*lower_limit).*getter)())) and
        (not upper_limit or (arg <= ((*upper_limit).*getter)())))
    {
      return arg;
    }

    std::string sep;
    std::ostringstream msg;
    msg << "Value '" << arg << "' is out of the allowed range (";
    if (lower_limit)
    {
      msg << sep << "lowerLimit=" << ((*lower_limit).*getter)();
      sep = ", ";
    }
    if (upper_limit)
    {
      msg << sep << "upperLimit=" << ((*upper_limit).*getter)();
    }
    msg << ")";
    throw Fmi::Exception(BCP, msg.str()).disableStackTrace();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <typename NumType>
bool inside_limits_impl(NumType arg,
                        const boost::optional<Value>& lower_limit,
                        const boost::optional<Value>& upper_limit,
                        NumType (Value::*getter)() const)
{
  try
  {
    return ((not lower_limit or (arg >= ((*lower_limit).*getter)())) and
            (not upper_limit or (arg <= ((*upper_limit).*getter)())));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

namespace SmartMet
{
namespace Spine
{
void Value::reset()
{
  try
  {
    Value::Empty empty;
    data = empty;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const Value& Value::check_limits(const boost::optional<Value>& lower_limit,
                                 const boost::optional<Value>& upper_limit) const
{
  try
  {
    switch (data.which())
    {
      case TI_EMPTY:
      case TI_BOOL:
      case TI_STRING:
      default:
        // Do not check anything for these types
        return *this;

      case TI_INT:
        check_limits_impl(get_int(), lower_limit, upper_limit, &Value::get_int);
        return *this;

      case TI_UINT:
        check_limits_impl(get_uint(), lower_limit, upper_limit, &Value::get_uint);
        return *this;

      case TI_DOUBLE:
        check_limits_impl(get_double(), lower_limit, upper_limit, &Value::get_double);
        return *this;

      case TI_PTIME:
        check_limits_impl(get_ptime_ext(), lower_limit, upper_limit, &Value::get_ptime_ext);
        return *this;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Value::inside_limits(const boost::optional<Value>& lower_limit,
                          const boost::optional<Value>& upper_limit) const
{
  try
  {
    switch (data.which())
    {
      case TI_EMPTY:
      case TI_BOOL:
      case TI_STRING:
      default:
        // Only report an error about not supported type if the actual
        // range check is required
        if (lower_limit or upper_limit)
        {
          std::ostringstream msg;
          msg << "Usupported type '" << demangle_cpp_type_name(data.type().name()) << "'";
          throw Fmi::Exception(BCP, msg.str());
        }

        return true;

      case TI_INT:
        return inside_limits_impl(get_int(), lower_limit, upper_limit, &Value::get_int);

      case TI_UINT:
        return inside_limits_impl(get_uint(), lower_limit, upper_limit, &Value::get_uint);

      case TI_DOUBLE:
        return inside_limits_impl(get_double(), lower_limit, upper_limit, &Value::get_double);

      case TI_PTIME:
        return inside_limits_impl(get_ptime_ext(), lower_limit, upper_limit, &Value::get_ptime_ext);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Value::get_bool() const
{
  try
  {
    switch (data.which())
    {
      case TI_BOOL:
        return boost::get<bool>(data);

      case TI_INT:
        return boost::get<int64_t>(data) != 0;

      case TI_UINT:
        return boost::get<uint64_t>(data) != 0;

      case TI_STRING:
        return string2bool(boost::get<std::string>(data));

      default:
        bad_value_type(METHOD_NAME, typeid(bool));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int64_t Value::get_int() const
{
  try
  {
    uint64_t tmp;
    switch (data.which())
    {
      case TI_INT:
        return boost::get<int64_t>(data);

      case TI_UINT:
        tmp = boost::get<uint64_t>(data);
        if (tmp > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        {
          std::ostringstream msg;
          msg << "The value '" << tmp << "' is too large for int64_t";
          throw Fmi::Exception(BCP, msg.str()).disableStackTrace();
        }
        return static_cast<int64_t>(tmp);

      case TI_STRING:
        try
        {
          return Fmi::stol(boost::get<std::string>(data));
        }
        catch (const std::exception&)
        {
          std::ostringstream msg;
          msg << "Failed to read an integer value from the string '"
              << boost::get<std::string>(data) << "'!";
          throw Fmi::Exception(BCP, msg.str()).disableStackTrace();
        }

      default:
        bad_value_type(METHOD_NAME, typeid(int64_t));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

uint64_t Value::get_uint() const
{
  try
  {
    int64_t tmp;

    switch (data.which())
    {
      case TI_UINT:
        return boost::get<uint64_t>(data);

      case TI_INT:
        tmp = boost::get<int64_t>(data);
        if (tmp < 0)
        {
          std::ostringstream msg;
          msg << "Cannot assign negative value " << tmp << " to uint64_t!";
          throw Fmi::Exception(BCP, msg.str()).disableStackTrace();
        }
        return static_cast<uint64_t>(tmp);

      case TI_STRING:
        try
        {
          return Fmi::stoul(boost::get<std::string>(data));
        }
        catch (const std::exception&)
        {
          std::ostringstream msg;
          msg << "Failed to read an unsigned integer value from the string '"
              << boost::get<std::string>(data) << "'!";
          throw Fmi::Exception(BCP, msg.str()).disableStackTrace();
        }

      default:
        bad_value_type(METHOD_NAME, typeid(uint64_t));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Value::get_double() const
{
  try
  {
    switch (data.which())
    {
      case TI_INT:
        return static_cast<double>(boost::get<int64_t>(data));

      case TI_UINT:
        return static_cast<double>(boost::get<uint64_t>(data));

      case TI_DOUBLE:
        return boost::get<double>(data);

      case TI_STRING:
        try
        {
          return Fmi::stod(Fmi::trim_copy(boost::get<std::string>(data)));
        }
        catch (const std::exception&)
        {
          std::ostringstream msg;
          msg << "Failed to read a double value from the string '" << boost::get<std::string>(data)
              << "'";
          throw Fmi::Exception(BCP, msg.str()).disableStackTrace();
        }

      default:
        bad_value_type(METHOD_NAME, typeid(double));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime Value::get_ptime(bool use_extensions) const
{
  try
  {
    switch (data.which())
    {
      case TI_PTIME:
        return boost::get<boost::posix_time::ptime>(data);

      case TI_STRING:
        if (use_extensions)
        {
          return string2ptime(boost::get<std::string>(data));
        }
        else
        {
          return Fmi::TimeParser::parse(boost::get<std::string>(data));
        }

      default:
        bad_value_type(METHOD_NAME, typeid(boost::posix_time::ptime));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Point Value::get_point() const
{
  try
  {
    int ind = data.which();
    if (ind == TI_POINT)
      return boost::get<PointWrapper>(data);

    if (ind == TI_STRING)
      return Point(boost::get<std::string>(data));

    bad_value_type(METHOD_NAME, typeid(Point));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

BoundingBox Value::get_bbox() const
{
  try
  {
    int ind = data.which();
    if (ind == TI_BBOX)
      return boost::get<BoundingBox>(data);

    if (ind == TI_STRING)
      return BoundingBox(boost::get<std::string>(data));

    bad_value_type(METHOD_NAME, typeid(BoundingBox));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string Value::get_string() const
{
  try
  {
    switch (data.which())
    {
      case TI_STRING:
        return boost::get<std::string>(data);

        // FIXME: Do we need conversions from other types back to string?

      default:
        bad_value_type(METHOD_NAME, typeid(std::string));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Value Value::from_config(libconfig::Setting& setting)
{
  try
  {
    Value result;
    switch (setting.getType())
    {
      case libconfig::Setting::TypeBoolean:
        result.set_bool(setting);
        break;

      case libconfig::Setting::TypeInt:
        // Setting allows only int export, Value only int64_t export
        result.set_int(static_cast<int64_t>(static_cast<int>(setting)));
        break;

      case libconfig::Setting::TypeInt64:
        result.set_int(setting);
        break;

      case libconfig::Setting::TypeFloat:
        result.set_double(setting);
        break;

      case libconfig::Setting::TypeString:
        result.set_string(setting);
        break;

      case libconfig::Setting::TypeNone:
      case libconfig::Setting::TypeGroup:
      case libconfig::Setting::TypeArray:
      case libconfig::Setting::TypeList:
      {
        std::ostringstream msg;
        msg << "Only scalar values are supported.\n"
            << "Got (at '" << setting.getPath() << "'):\n";
        Spine::ConfigBase::dump_setting(msg, setting, 16);
        throw Fmi::Exception(BCP, msg.str());
      }
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Value Value::from_config(libconfig::Config& config, const std::string& path)
{
  try
  {
    libconfig::Setting& setting = config.lookup(path);
    return from_config(setting);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Value Value::from_config(libconfig::Config& config,
                         const std::string& path,
                         const Value& default_value)
{
  try
  {
    if (!config.exists(path))
      return default_value;
    return from_config(config, path);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string Value::dump_to_string() const
{
  try
  {
    namespace pt = boost::posix_time;
    namespace ba = boost::algorithm;

    switch (data.which())
    {
      case TI_EMPTY:
        return "(empty)";

      case TI_BOOL:
        return fmt::format("(bool {})", (boost::get<bool>(data) ? "true" : "false"));
        break;

      case TI_INT:
        return fmt::format("(int {})", boost::get<int64_t>(data));

      case TI_UINT:
        return fmt::format("(uint {})", boost::get<uint64_t>(data));

      case TI_DOUBLE:
        return ba::trim_right_copy(fmt::format("(double {:<-14g})", boost::get<double>(data)));

      case TI_STRING:
        return fmt::format("(string '{}')", boost::get<std::string>(data));

      case TI_PTIME:
        return fmt::format("(time '{}')", pt::to_simple_string(boost::get<pt::ptime>(data)));

      case TI_POINT:
        return fmt::format("(point {})", boost::get<PointWrapper>(data).as_string());

      case TI_BBOX:
        return fmt::format("(bbox {})", boost::get<BoundingBox>(data).as_string());

      default:
        throw Fmi::Exception(BCP, "INTERNAL ERROR: unrecognized type code");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Value::print_on(std::ostream& output) const
{
  try
  {
    output << dump_to_string();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string Value::to_string() const
{
  try
  {
    namespace pt = boost::posix_time;
    namespace ba = boost::algorithm;

    switch (data.which())
    {
      case TI_EMPTY:
        throw Fmi::Exception(BCP, "Uninitialized value");

      case TI_BOOL:
        return boost::get<bool>(data) ? "true" : "false";

      case TI_INT:
        return fmt::format("{}", boost::get<int64_t>(data));

      case TI_UINT:
        return fmt::format("{}", boost::get<uint64_t>(data));

      case TI_DOUBLE:
          return fmt::format("{:<.14g}", boost::get<double>(data));

      case TI_STRING:
        return boost::get<std::string>(data);

      case TI_PTIME:
        return Fmi::to_iso_string(boost::get<pt::ptime>(data)) + "Z";

      case TI_POINT:
        return boost::get<PointWrapper>(data).as_string();

      case TI_BBOX:
        return boost::get<BoundingBox>(data).as_string();

      default:
        throw Fmi::Exception(BCP, "INTERNAL ERROR: unrecognized type code");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Value::bad_value_type(const std::string& location, const std::type_info& exp_type) const
{
  try
  {
    std::ostringstream msg;
    msg << location << ": conversion from " << demangle_cpp_type_name(data.type().name()) << " to "
        << demangle_cpp_type_name(exp_type.name())
        << " is not supported (value=" << dump_to_string() << ")";
    throw Fmi::Exception(BCP, msg.str());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Value::get_not_implemented_for(const std::type_info& type) const
{
  try
  {
    std::ostringstream msg;
    msg << "<" << demangle_cpp_type_name(type.name()) << ">() is not implemented";
    throw Fmi::Exception(BCP, msg.str());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime string2ptime(const std::string& value,
                                      const boost::optional<boost::posix_time::ptime>& ref_time)
{
  try
  {
    namespace bl = boost::lambda;
    namespace ns = boost::spirit::standard;
    namespace qi = boost::spirit::qi;
    namespace pt = boost::posix_time;

    using qi_rule = qi::rule<std::string::const_iterator, std::string()>;

    unsigned unit_coeff = 1;
    unsigned num_units = 1;
    int direction = 0;
    bool rounded_up = false;
    unsigned rounded_min = 0;

    const std::string tmp = Fmi::ascii_tolower_copy(value);

    qi_rule round_dir_p = (+ns::space >> ns::string("up"))[bl::var(rounded_up) = true] |
                          (+ns::space >> ns::string("down"))[bl::var(rounded_up) = false] |
                          qi::eps[bl::var(rounded_up) = false];

    qi_rule rounded_minutes_p = ns::string("rounded") >> round_dir_p >> +ns::space >>
                                qi::uint_[bl::var(rounded_min) = bl::_1] >> +ns::space >>
                                (ns::string("minutes") | ns::string("minute") | ns::string("min"));

    qi_rule cond_rounded_p =
        ((+ns::space >> rounded_minutes_p) | qi::eps[bl::var(rounded_min) = 0]);

    qi_rule now_p =
        ns::string("now")[bl::var(unit_coeff) = 0, bl::var(num_units) = 1] >> cond_rounded_p;

    // Let us be lazy and accept for example '1 hours ago' or 'hours ago' as '1 hour ago'
    qi_rule unit_p = (ns::string("second")[bl::var(unit_coeff) = 1] ||
                      ns::string("minute")[bl::var(unit_coeff) = 60] ||
                      ns::string("hour")[bl::var(unit_coeff) = 3600] ||
                      ns::string("day")[bl::var(unit_coeff) = 86400]) >>
                     -qi::char_('s');

    qi_rule units_ago_p = (-(qi::uint_[bl::var(num_units) = bl::_1])) >> +ns::space >> unit_p >>
                          +ns::space >> ns::string("ago")[bl::var(direction) = -1] >>
                          cond_rounded_p;

    qi_rule after_units_p = ns::string("after")[bl::var(direction) = +1] >> +ns::space >>
                            (-(qi::uint_[bl::var(num_units) = bl::_1])) >> +ns::space >> unit_p >>
                            cond_rounded_p;

    if (qi::phrase_parse(tmp.begin(), tmp.end(), (now_p | units_ago_p | after_units_p), ns::space))
    {
      if (rounded_min != 0)
      {
        if (1440 % rounded_min != 0)
        {
          std::ostringstream msg;
          msg << "Invalid request to round to full minutes in '" << value << "'";
          throw Fmi::Exception(BCP, msg.str());
        }
      }

      pt::ptime t1 = ref_time ? *ref_time : pt::second_clock::universal_time();
      // FIXME: should we care about possible overflow here?
      int offset = static_cast<int>(num_units * unit_coeff);
      pt::ptime t2 = t1 + pt::seconds(direction * offset);

      if (rounded_min != 0)
      {
        unsigned rounded_sec = 60 * rounded_min;
        long off = rounded_up ? rounded_sec - 1 : 0;
        long sec = rounded_sec * ((t2.time_of_day().total_seconds() + off) / rounded_sec);
        auto d = t2.date();
        t2 = pt::ptime(d, pt::seconds(sec));
      }

      return t2;
    }

    auto t2 = parse_xml_time(tmp);
    if (t2.is_not_a_date_time())
      return Fmi::TimeParser::parse(value);
    return t2;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").disableStackTrace();  // user input error
  }
}

boost::posix_time::ptime parse_xml_time(const std::string& value)
{
  try
  {
    namespace bl = boost::lambda;
    namespace ns = boost::spirit::standard;
    namespace qi = boost::spirit::qi;
    namespace bg = boost::gregorian;
    namespace pt = boost::posix_time;

    using qi_rule = qi::rule<std::string::const_iterator, std::string()>;

    const std::string tmp = Fmi::trim_copy(value);

    char tz_off_sign = ' ';
    short unsigned year = 0;
    short unsigned month = 0;
    short unsigned day = 0;
    unsigned hours = 0;
    unsigned minutes = 0;
    unsigned seconds = 0;
    unsigned off_hours = 0;
    unsigned off_min = 0;

    qi::uint_parser<int, 10, 4, 4> uint4_p;
    qi::uint_parser<unsigned, 10, 2, 2> uint2_p;

    qi_rule date_p = uint4_p[bl::var(year) = bl::_1] >> ns::char_('-') >>
                     uint2_p[bl::var(month) = bl::_1] >> ns::char_('-') >>
                     uint2_p[bl::var(day) = bl::_1] >> ns::char_('T');

    qi_rule time_p =
        uint2_p[bl::var(hours) = bl::_1] >> ns::char_(':') >> uint2_p[bl::var(minutes) = bl::_1] >>
        ((ns::char_(':') >> uint2_p[bl::var(seconds) = bl::_1]) | qi::eps[bl::var(seconds) = 0]);

    qi_rule tz_p =
        ns::char_('Z')[bl::var(off_hours) = 0, bl::var(off_min) = 0, bl::var(tz_off_sign) = '+'] |
        (ns::char_("+-")[bl::var(tz_off_sign) = bl::_1] >> uint2_p[bl::var(off_hours) = bl::_1] >>
         ns::char_(':') >> uint2_p[bl::var(off_min) = bl::_1]);

    qi_rule date_time_p =
        date_p >> time_p >> (tz_p | qi::eps[bl::var(tz_off_sign) = ' ']) >> qi::eoi;

    if (qi::phrase_parse(tmp.begin(), tmp.end(), date_time_p, ns::space))
    {
      if (tz_off_sign == ' ')
      {
        std::ostringstream msg;
        msg << "Time zone not provided in '" << tmp << "'";
        throw Fmi::Exception(BCP, msg.str());
      }

      try
      {
        bg::date dt(year, month, day);
        pt::time_duration part_of_day(
            static_cast<int>(hours), static_cast<int>(minutes), static_cast<int>(seconds), 0);
        pt::ptime t1(dt, part_of_day);
        int offset = (tz_off_sign == '+' ? 1 : -1) * static_cast<int>(60 * off_hours + off_min);
        t1 -= pt::minutes(offset);
        return t1;
      }
      catch (const std::exception& err)
      {
        std::ostringstream msg;
        msg << "Failed to read time from the string '" << tmp << "': " << err.what();
        throw Fmi::Exception(BCP, msg.str()).disableStackTrace();
      }
    }
    else
      return {};
  }
  catch (...)
  {
     auto error = Fmi::Exception::Trace(BCP, "Failed to parse XML time");
     error.addParameter("Argument", value);
     throw error;
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// TODO(mheiskan) Fix the API to use a const reference
bool string2bool(const std::string& src)
{
  try
  {
    std::string arg = src;
    Fmi::ascii_tolower(arg);
    Fmi::trim(arg);

    if ((arg == "true") || (arg == "1"))
      return true;

    if ((arg == "false") || (arg == "0"))
      return false;

    std::ostringstream msg;
    msg << "Cannot convert '" << src << "' to bool.";
    throw Fmi::Exception(BCP, msg.str()).disableStackTrace();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
bool Value::get() const
{
  try
  {
    return get_bool();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
int64_t Value::get() const
{
  try
  {
    return get_int();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
uint64_t Value::get() const
{
  try
  {
    return get_uint();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
double Value::get() const
{
  try
  {
    return get_double();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
std::string Value::get() const
{
  try
  {
    return get_string();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
boost::posix_time::ptime Value::get() const
{
  try
  {
    return get_ptime();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
Point Value::get() const
{
  try
  {
    return get_point();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
BoundingBox Value::get() const
{
  try
  {
    return get_bbox();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
Value Value::get() const
{
  try
  {
    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Point::parse_string(const std::string& src)
{
  try
  {
    namespace ba = boost::algorithm;

    std::vector<std::string> parts;
    ba::split(parts, src, ba::is_any_of(","));
    if ((parts.size() < 2) or (parts.size() > 3))
    {
      std::ostringstream msg;
      msg << "Invalid point format in '" << src << "' (x,y[,crs]) expected)";
      throw Fmi::Exception(BCP, msg.str());
    }

    crs = parts.size() == 2 ? std::string("") : Fmi::trim_copy(parts[2]);
    x = Fmi::stod(Fmi::trim_copy(parts[0]));
    y = Fmi::stod(Fmi::trim_copy(parts[1]));
  }
  catch (...)
  {
    auto error = Fmi::Exception::Trace(BCP, "Failed to parse point value");
    error.addParameter("Argument", src);
    throw error;
  }
}

std::string Point::as_string() const
{
  try
  {
    const std::string result = fmt::format("{:<.14g} {:<.14g} {}", x, y, crs);
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Point::operator==(const Point& p) const
{
  try
  {
    namespace ba = boost::algorithm;
    return x == p.x and y == p.y and ba::iequals(crs, p.crs);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void BoundingBox::parse_string(const std::string& src)
{
  try
  {
    // Split into max 5 parts, the optional 5th one is the CRS
    auto n = std::count(src.begin(), src.end(), ',');
    if (n < 3)
      throw Fmi::Exception(BCP, "Invalid bounding box format, expecting xmin,ymin,xmax,ymax[,crs]")
          .addParameter("bbox", src);

    auto pos1 = src.find(',');
    auto pos2 = src.find(',', pos1 + 1);
    auto pos3 = src.find(',', pos2 + 1);

    xMin = Fmi::stod(Fmi::trim_copy(src.substr(0, pos1)));
    yMin = Fmi::stod(Fmi::trim_copy(src.substr(pos1 + 1, pos2 - pos1 - 1)));
    xMax = Fmi::stod(Fmi::trim_copy(src.substr(pos2 + 1, pos3 - pos2 - 1)));
    if (n == 3)
    {
      yMax = Fmi::stod(Fmi::trim_copy(src.substr(pos3 + 1, std::string::npos)));
      crs = "";
    }
    else
    {
      auto pos4 = src.find(',', pos3 + 1);
      yMax = Fmi::stod(Fmi::trim_copy(src.substr(pos3 + 1, pos4 - pos3 - 1)));
      crs = Fmi::trim_copy(src.substr(pos4 + 1, std::string::npos));
    }
  }
  catch (...)
  {
     auto error = Fmi::Exception::Trace(BCP, "Failed to parse bounding box!");
     error.addParameter("Argument", src);
     throw error;
  }
}

std::string BoundingBox::as_string() const
{
  try
  {
    return fmt::format("{:<.14g} {:<.14g} {:<.14g} {:<.14g} {}", xMin, yMin, xMax, yMax, crs);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool BoundingBox::operator==(const BoundingBox& b) const
{
  try
  {
    namespace ba = boost::algorithm;
    return xMin == b.xMin and yMin == b.yMin and xMax == b.xMax and yMax == b.yMax and
           ba::iequals(crs, b.crs);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
