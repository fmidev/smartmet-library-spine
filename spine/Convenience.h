// ======================================================================
/*!
 * \brief Convenience functions
 */
// ======================================================================
#pragma once

#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>

#include <boost/any.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>

#include <boost/any.hpp>
#include <boost/optional.hpp>
#include <functional>
#include <locale>
#include <stdexcept>
#include <string>

namespace SmartMet
{
namespace Spine
{
class ValueFormatter;

std::string optional_string(const char* theValue, const std::string& theDefault);
std::string optional_string(const boost::optional<std::string>& theValue,
                            const std::string& theDefault);
bool optional_bool(const char* theValue, bool theDefault);
bool optional_bool(const boost::optional<std::string>& theValue, bool theDefault);
int optional_int(const char* theValue, int theDefault);
int optional_int(const boost::optional<std::string>& theValue, int theDefault);
unsigned long optional_unsigned_long(const char* theValue, unsigned long theDefault);
unsigned long optional_unsigned_long(const boost::optional<std::string>& theValue,
                                     unsigned long theDefault);
char optional_char(const char* theValue, char theDefault);
char optional_char(const boost::optional<std::string>& theValue, char theDefault);
double optional_double(const char* theValue, double theDefault);
double optional_double(const boost::optional<std::string>& theValue, double theDefault);
boost::posix_time::ptime optional_time(const char* theValue,
                                       const boost::posix_time::ptime& theDefault);
boost::posix_time::ptime optional_time(const boost::optional<std::string>& theValue,
                                       const boost::posix_time::ptime& theDefault);

std::string required_string(const char* theValue, const char* theError);
std::string required_string(const boost::optional<std::string>& theValue, const char* theError);
bool required_bool(const char* theValue, const char* theError);
bool required_bool(const boost::optional<std::string>& theValue, const char* theError);
int required_int(const char* theValue, const char* theError);
int required_int(const boost::optional<std::string>& theValue, const char* theError);
unsigned long required_unsigned_long(const char* theValue, const char* theError);
unsigned long required_unsigned_long(const boost::optional<std::string>& theValue,
                                     const char* theError);
char required_char(const char* theValue, const char* theError);
char required_char(const boost::optional<std::string>& theValue, const char* theError);
double required_double(const char* theValue, const char* theError);
double required_double(const boost::optional<std::string>& theValue, const char* theError);
boost::posix_time::ptime required_time(const char* theValue, const char* theError);
boost::posix_time::ptime required_time(const boost::optional<std::string>& theValue,
                                       const char* theError);

std::string htmlescape(const std::string& theValue);

/**
 *  @brief Returns content of boost::any as string
 */
std::string boost_any_to_string(const boost::any& anyvalue,
                                const ValueFormatter& vf,
                                int precision);

/**
 *  @brief Returns content of boost::any as string
 */
std::string boost_any_to_string(const boost::any& anyvalue);

/**
 *  @brief Converts duration string to minutes: e.g. 1m = 1, 1h == 60, 1d = 1440
 */
int duration_string_to_minutes(const std::string& interval);

/**
 *  @brief Returns current time as string for use in log messages
 */
std::string log_time_str();

/**
 *  @brief Case insensitive string comparisson (less)
 *
 *  FIXME: the comparison is still case sensitive for UTF-8 non-ASCII symbols
 */
bool str_iless(const std::string& a,
               const std::string& b,
               const std::locale& locale = std::locale());

/**
 *  @brief Case insensitive string comparisson (equal)
 *
 *  FIXME: the comparison is still case sensitive for UTF-8 non-ASCII symbols
 */
bool str_iequal(const std::string& a,
                const std::string& b,
                const std::locale& locale = std::locale());

struct CaseInsensitiveLess : public std::binary_function<std::string, std::string, bool>
{
  std::locale locale;

 public:
  CaseInsensitiveLess(const std::locale& locale = std::locale()) : locale(locale) {}
  bool operator()(const std::string& a, const std::string& b) const
  {
    return str_iless(a, b, locale);
  }
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
