// ======================================================================
/*!
 * \brief Implementation of class AsciiFormatter
 */
// ======================================================================

#include "Convenience.h"
#include "ValueFormatter.h"
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <stdexcept>
#include <string>

namespace pt = boost::posix_time;
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
    else
      return theValue;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string optional_string(const boost::optional<std::string>& theValue,
                            const std::string& theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    else
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
    else
      return (Fmi::stoi(theValue) != 0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool optional_bool(const boost::optional<std::string>& theValue, bool theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    else
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
    else
      return Fmi::stoi(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int optional_int(const boost::optional<std::string>& theValue, int theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    else
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
    else
      return Fmi::stoul(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t optional_size(const boost::optional<std::string>& theValue, std::size_t theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    else
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
    else
      return Fmi::stoul(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned long optional_unsigned_long(const boost::optional<std::string>& theValue,
                                     unsigned long theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    else
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
    else
      return std::string(theValue).at(0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

char optional_char(const boost::optional<std::string>& theValue, char theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    else
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
    else
      return Fmi::stod(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double optional_double(const boost::optional<std::string>& theValue, double theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    else
      return Fmi::stod(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime optional_time(const char* theValue,
                                       const boost::posix_time::ptime& theDefault)
{
  try
  {
    if (theValue == nullptr)
      return theDefault;
    else
      return Fmi::TimeParser::parse(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime optional_time(const boost::optional<std::string>& theValue,
                                       const boost::posix_time::ptime& theDefault)
{
  try
  {
    if (!theValue)
      return theDefault;
    else
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
  else
    return theValue;
}

std::string required_string(const boost::optional<std::string>& theValue, const char* theError)
{
  if (!theValue)
    throw Fmi::Exception(BCP, theError);
  else
    return *theValue;
}

bool required_bool(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    else
      return (Fmi::stoi(theValue) != 0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool required_bool(const boost::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    else
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
    else
      return Fmi::stoi(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int required_int(const boost::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    else
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
    else
      return Fmi::stoul(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t required_size(const boost::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    else
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
    else
      return Fmi::stoul(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned long required_unsigned_long(const boost::optional<std::string>& theValue,
                                     const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    else
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
    else
      return std::string(theValue).at(0);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

char required_char(const boost::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    else
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
    else
      return Fmi::stod(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double required_double(const boost::optional<std::string>& theValue, const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    else
      return Fmi::stod(*theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime required_time(const char* theValue, const char* theError)
{
  try
  {
    if (theValue == nullptr)
      throw Fmi::Exception(BCP, theError);
    else
      return Fmi::TimeParser::parse(theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::posix_time::ptime required_time(const boost::optional<std::string>& theValue,
                                       const char* theError)
{
  try
  {
    if (!theValue)
      throw Fmi::Exception(BCP, theError);
    else
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
    const pt::ptime now = pt::second_clock::local_time();
    return Fmi::to_simple_string(now);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string boost_any_to_string(const boost::any& anyvalue)
{
  try
  {
    ValueFormatterParam vfp;

    ValueFormatter vf(vfp);

    return boost_any_to_string(anyvalue, vf, 3);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string boost_any_to_string(const boost::any& anyvalue, const ValueFormatter& vf, int precision)
{
  try
  {
    std::string retval("");

    const std::type_info& type = anyvalue.type();

    // check if empty
    if (anyvalue.empty())
    {
      if (type == typeid(char*) || type == typeid(std::string))
        return retval;
      else
        return vf.missing();
    }

    if (type == typeid(char*))
      retval = boost::any_cast<const char*>(anyvalue);
    else if (type == typeid(short))
      retval = Fmi::to_string(boost::any_cast<short>(anyvalue));
    else if (type == typeid(unsigned short))
      retval = Fmi::to_string(static_cast<int>(boost::any_cast<unsigned short>(anyvalue)));
    else if (type == typeid(int))
      retval = Fmi::to_string(boost::any_cast<int>(anyvalue));
    else if (type == typeid(unsigned int))
      retval = Fmi::to_string(boost::any_cast<unsigned int>(anyvalue));
    else if (type == typeid(long))
      retval = Fmi::to_string(boost::any_cast<long>(anyvalue));
    else if (type == typeid(unsigned long))
      retval = Fmi::to_string(boost::any_cast<unsigned long>(anyvalue));
    else if (type == typeid(long long))
      retval = std::to_string(boost::any_cast<long long>(anyvalue));
    else if (type == typeid(unsigned long long))
      retval = std::to_string(boost::any_cast<unsigned long long>(anyvalue));
    else if (type == typeid(float))
    {
      float floatvalue(boost::any_cast<float>(anyvalue));
      retval = vf.format(static_cast<double>(floatvalue), precision);
    }
    else if (type == typeid(double))
    {
      double doublevalue(boost::any_cast<double>(anyvalue));
      retval = vf.format(doublevalue, precision);
    }
    else if (type == typeid(long double))
    {
      double doublevalue(static_cast<double>(boost::any_cast<long double>(anyvalue)));
      retval = vf.format(doublevalue, precision);
    }
    else if (type == typeid(std::string))
    {
      retval = boost::any_cast<std::string>(anyvalue);
    }
    else if (type == typeid(std::pair<double, double>))
    {
      auto value = boost::any_cast<std::pair<double, double> >(anyvalue);
      retval = vf.format(value.first, precision);
      retval += ',';
      retval += vf.format(value.second, precision);
    }
    else if (type == typeid(std::vector<boost::any>))
    {
      std::vector<boost::any> anyvector = boost::any_cast<std::vector<boost::any> >(anyvalue);

      bool matrix_data(anyvector.size() > 1);
      if (matrix_data)
        retval.append("[");

      for (unsigned int i = 0; i < anyvector.size(); i++)
      {
        if ((matrix_data && retval.size() > 1) || (!matrix_data && retval.size() > 0))
          retval.append(" ");
        retval.append(boost_any_to_string(anyvector[i], vf, precision));
      }

      if (matrix_data)
        retval.append("]");
    }
    else
    {
      throw Fmi::Exception(BCP, "Unknown boost::any datatype: " + std::string(type.name()));
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
