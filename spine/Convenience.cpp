// ======================================================================
/*!
 * \brief Implementation of class AsciiFormatter
 */
// ======================================================================

#include "Convenience.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/ValueFormatter.h>
#include <stdexcept>
#include <string>

namespace ba = boost::algorithm;

namespace SmartMet
{
namespace Spine
{
std::string optional_string(const char* theValue, const std::string& theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    return theValue;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string optional_string(const std::optional<std::string>& theValue,
                            const std::string& theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    return *theValue;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool optional_bool(const char* theValue, bool theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    return (Fmi::stoi(theValue) != 0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool optional_bool(const std::optional<std::string>& theValue, bool theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    return (Fmi::stoi(*theValue) != 0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int optional_int(const char* theValue, int theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    return Fmi::stoi(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int optional_int(const std::optional<std::string>& theValue, int theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    return Fmi::stoi(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t optional_size(const char* theValue, std::size_t theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    return Fmi::stoul(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t optional_size(const std::optional<std::string>& theValue, std::size_t theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    return Fmi::stoul(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned long optional_unsigned_long(const char* theValue, unsigned long theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    return Fmi::stoul(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned long optional_unsigned_long(const std::optional<std::string>& theValue,
                                     unsigned long theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    return Fmi::stoul(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

char optional_char(const char* theValue, char theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    return std::string(theValue).at(0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

char optional_char(const std::optional<std::string>& theValue, char theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    return std::string(*theValue).at(0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double optional_double(const char* theValue, double theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    return Fmi::stod(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double optional_double(const std::optional<std::string>& theValue, double theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    return Fmi::stod(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Fmi::DateTime optional_time(const char* theValue,
                                       const Fmi::DateTime& theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    return Fmi::TimeParser::parse(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Fmi::DateTime optional_time(const std::optional<std::string>& theValue,
                                       const Fmi::DateTime& theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    return Fmi::TimeParser::parse(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string required_string(const char* theValue, const char* theError)
{
  if (theValue == nullptr)
    throw Fmi::Exception(BCP, theError);
  return theValue;
}

std::string required_string(const std::optional<std::string>& theValue, const char* theError)
{
  if (!theValue)
    throw Fmi::Exception(BCP, theError);
  return *theValue;
}

bool required_bool(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    return (Fmi::stoi(theValue) != 0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool required_bool(const std::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    return (Fmi::stoi(*theValue) != 0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int required_int(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    return Fmi::stoi(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int required_int(const std::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    return Fmi::stoi(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t required_size(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    return Fmi::stoul(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t required_size(const std::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    return Fmi::stoul(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned long required_unsigned_long(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    return Fmi::stoul(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned long required_unsigned_long(const std::optional<std::string>& theValue,
                                     const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    return Fmi::stoul(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

char required_char(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    return std::string(theValue).at(0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

char required_char(const std::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    return std::string(*theValue).at(0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double required_double(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    return Fmi::stod(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double required_double(const std::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    return Fmi::stod(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Fmi::DateTime required_time(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    return Fmi::TimeParser::parse(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Fmi::DateTime required_time(const std::optional<std::string>& theValue,
                                       const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    return Fmi::TimeParser::parse(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// https://stackoverflow.com/questions/5665231/most-efficient-way-to-escape-xml-html-in-c-string
std::string htmlescape(const std::string& theValue)
{
  std::string buffer;
  auto n = theValue.size();
  buffer.reserve(n);
  for (std::size_t pos = 0; pos < n; ++pos)
  {
    switch (theValue[pos])
    {
      case '&':
        buffer += "&amp;";
        break;
      case '\"':
        buffer += "&quot;";
        break;
      case '\'':
        buffer += "&apos;";
        break;
      case '<':
        buffer += "&lt;";
        break;
      case '>':
        buffer += "&gt;";
        break;
      default:
        buffer += theValue[pos];
        break;
    }
  }
  return buffer;
}

bool str_iless(const std::string& a, const std::string& b, const std::locale& locale)
{
  try
  {
    return ba::ilexicographical_compare(a, b, locale);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool str_iequal(const std::string& a, const std::string& b, const std::locale& locale)
{
  try
  {
    return ba::iequals(a, b, locale);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string log_time_str()
{
  try
  {
    const Fmi::DateTime now = Fmi::SecondClock::local_time();
    return Fmi::to_simple_string(now);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string boost_any_to_string(const std::any& anyvalue)
{
  try
  {
    Fmi::ValueFormatterParam vfp;
    Fmi::ValueFormatter vf(vfp);

    return boost_any_to_string(anyvalue, vf, 3);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string boost_any_to_string(const std::any& anyvalue,
                                const Fmi::ValueFormatter& vf,
                                int precision)
{
  try
  {
    std::string retval;

    const std::type_info& type = anyvalue.type();

    // check if empty
    if (!anyvalue.has_value())
    {
      if (type == typeid(char*) || type == typeid(std::string))
        return retval;
      return vf.missing();
    }

    if (type == typeid(char*))
      retval = std::any_cast<const char*>(anyvalue);
    else if (type == typeid(short))
      retval = Fmi::to_string(std::any_cast<short>(anyvalue));
    else if (type == typeid(unsigned short))
      retval = Fmi::to_string(static_cast<int>(std::any_cast<unsigned short>(anyvalue)));
    else if (type == typeid(int))
      retval = Fmi::to_string(std::any_cast<int>(anyvalue));
    else if (type == typeid(unsigned int))
      retval = Fmi::to_string(std::any_cast<unsigned int>(anyvalue));
    else if (type == typeid(long))
      retval = Fmi::to_string(std::any_cast<long>(anyvalue));
    else if (type == typeid(unsigned long))
      retval = Fmi::to_string(std::any_cast<unsigned long>(anyvalue));
    else if (type == typeid(long long))
      retval = std::to_string(std::any_cast<long long>(anyvalue));
    else if (type == typeid(unsigned long long))
      retval = std::to_string(std::any_cast<unsigned long long>(anyvalue));
    else if (type == typeid(float))
    {
      auto floatvalue = std::any_cast<float>(anyvalue);
      retval = vf.format(static_cast<double>(floatvalue), precision);
    }
    else if (type == typeid(double))
    {
      auto doublevalue = std::any_cast<double>(anyvalue);
      retval = vf.format(doublevalue, precision);
    }
    else if (type == typeid(long double))
    {
      auto doublevalue = static_cast<double>(std::any_cast<long double>(anyvalue));
      retval = vf.format(doublevalue, precision);
    }
    else if (type == typeid(std::string))
    {
      retval = std::any_cast<std::string>(anyvalue);
    }
    else if (type == typeid(std::pair<double, double>))
    {
      auto value = std::any_cast<std::pair<double, double> >(anyvalue);
      retval = vf.format(value.first, precision);
      retval += ',';
      retval += vf.format(value.second, precision);
    }
    else if (type == typeid(std::vector<std::any>))
    {
      auto anyvector = std::any_cast<std::vector<std::any> >(anyvalue);

      bool matrix_data(anyvector.size() > 1);
      if (matrix_data)
        retval.append("[");

      for (const auto& value : anyvector)
      {
        if ((matrix_data && retval.size() > 1) || (!matrix_data && !retval.empty()))
          retval.append(" ");
        retval.append(boost_any_to_string(value, vf, precision));
      }

      if (matrix_data)
        retval.append("]");
    }
    else
    {
      throw Fmi::Exception(BCP, "Unknown std::any datatype: " + std::string(type.name()));
    }

    if (retval.empty() && !(type == typeid(char*) || type == typeid(std::string)))
      retval = vf.missing();

    return retval;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int duration_string_to_minutes(const std::string& interval)
{
  try
  {
    int retval(-1);

    std::string interval_string(interval);

    char unit_char('m');

    if (interval.size() > 1 && !isdigit(interval_string.at(interval_string.size() - 1)))
    {
      unit_char = interval_string.at(interval_string.size() - 1);
      interval_string.resize(interval_string.size() - 1);
    }

    int duration = Fmi::stoi(interval_string);

    switch (unit_char)
    {
      case 'm':
        retval = duration;
        break;
      case 'h':
        retval = duration * 60;
        break;
      case 'd':
        retval = duration * 1440;
        break;
      default:
      {
        std::string err_str("'");
        err_str.push_back(unit_char);
        err_str += "' suffix is not valid, use 'm', 'h' or 'd' suffix instead!";
        throw Fmi::Exception(BCP, err_str);
      }
    }

    return retval;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
