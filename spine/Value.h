#pragma once

#include <macgyver/DateTime.h>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <libconfig.h++>
#include <ostream>
#include <typeinfo>

namespace SmartMet
{
namespace Spine
{
/**
 *   @brief Read Fmi::DateTime from std::string
 *
 *   Supports following formats:
 *   - one supported by Fmi::TimeParser::parse
 *   - string @b now means current time
 *   - strings like '3 hours ago', '60 minutes ago'. Also rounding both up and down is supported
 *     for relative time specification. Some examples:
 *        - 1 hour ago rounded 5 min
 *        - 1 hour ago rounded down 5 min
 *        - after 2 hours rounded up 15 minutes
 *     The default is to round downward unless @b up is explicitly specified. Only values
 *     in minutes are supported for rounding specification.
 *
 *   If the relative is used (for example '12 hours ago') specified value
 *   if the second parameter means time relative to which to calculate
 *   the offset.
 *
 *   FIXME: Tämä ei varmaan ole paras paikka. Value luokka kuitenkin käyttää sen:
 *          Se olisi varmaan paremmin siirtää sen mualle
 */
Fmi::DateTime string2ptime(const std::string& value,
                                      const boost::optional<Fmi::DateTime>& ref_time =
                                          boost::optional<Fmi::DateTime>());

/**
 *   @brief Parses XML time from string
 *
 *   Input format
 *   @verbatim
 *   ([-]CCYY-MM-DDThh:mm:ss[Z|(+|-)hh:mm])
 *   @endverbatim
 *
 *   returns
 *   - undefined ptime (ptime::is_not_a_date_time() returns true if format is not recognized
 *   - ptime (UTC) value in case of an success
 *
 *   throws std::runtime_error if format is recognized but read data are invalid or
 *   time zone is not specified (one of 'Z', '+HH:MM' and '-HH::MM' must be specified)
 */
Fmi::DateTime parse_xml_time(const std::string& value);

/**
 *   @brief Read bool from string
 *
 *   boost::lexical_cast<bool>() does not seem to work nicely
 *
 *   FIXME: Tämä ei varmaan ole paras paikka. Value luokka kuitenkin käyttää sen:
 *          Se olisi varmaan paremmin siirtää sen mualle
 */
bool string2bool(const std::string& src);

/**
 *   @brief Minimal implementation of 2D point for use in Value
 */
struct Point
{
  double x = 0.0;
  double y = 0.0;
  std::string crs;

  inline Point() = default;
  explicit inline Point(const std::string& src) { parse_string(src); }
  inline Point(double x, double y, const std::string& crs = "EPSG:4326") : x(x), y(y), crs(crs) {}
  void parse_string(const std::string& src);

  std::string as_string() const;

  bool operator==(const Point& p) const;
};

/**
 *   @brief Minimal implementation of 2D bounding box for use in Value
 */
struct BoundingBox
{
  double xMin = 0.0;
  double yMin = 0.0;
  double xMax = 0.0;
  double yMax = 0.0;
  std::string crs;

  inline BoundingBox() = default;
  explicit inline BoundingBox(const std::string& src) { parse_string(src); }
  inline BoundingBox(
      double xMin, double yMin, double xMax, double yMax, std::string crs = "EPSG:4326")
      : xMin(xMin), yMin(yMin), xMax(xMax), yMax(yMax), crs(std::move(crs))
  {
  }

  void parse_string(const std::string& src);

  std::string as_string() const;

  bool operator==(const BoundingBox& b) const;
};

/**
 *   @brief Class Value acts as container for basic types required for handling
 *          scalar types in configuration files and request parameters
 */
class Value
{
  /**
   *  @brief Enum for type index for date
   *
   *  The enum values must correspond to the order of actual types in
   *  boost\::variant Value::data.
   */
  enum TypeIndex
  {
    TI_EMPTY = 0,
    TI_BOOL,
    TI_INT,
    TI_UINT,
    TI_DOUBLE,
    TI_STRING,
    TI_PTIME,
    TI_POINT,
    TI_BBOX
  };

  struct Empty
  {
    inline bool operator==(const Empty&) const { return true; }
  };

  /**
   *  This ugly structure is here only to workaround gcc-4.4
   *  strict aliasing warning which happens to be generated
   *  if size is not the same as one of BoundingBox
   */
  struct PointWrapper : public Point
  {
    char unused[16];

    PointWrapper(const Point& p) : Point(p) {}
  };

  boost::variant<Empty,
                 bool,
                 int64_t,
                 uint64_t,
                 double,
                 std::string,
                 Fmi::DateTime,
                 PointWrapper,
                 BoundingBox>
      data;

 public:
  Value() = default;

  inline explicit Value(bool x) { set_bool(x); }
  inline explicit Value(signed char x) { set_int(x); }
  inline explicit Value(signed short int x) { set_int(x); }
  inline explicit Value(signed int x) { set_int(x); }
  inline explicit Value(signed long int x) { set_int(x); }
  inline explicit Value(signed long long int x) { set_int(x); }
  inline explicit Value(unsigned char x) { set_uint(x); }
  inline explicit Value(unsigned short int x) { set_uint(x); }
  inline explicit Value(unsigned int x) { set_uint(x); }
  inline explicit Value(unsigned long int x) { set_uint(x); }
  inline explicit Value(unsigned long long int x) { set_uint(x); }
  inline explicit Value(float x) { set_double(static_cast<double>(x)); }
  inline explicit Value(double x) { set_double(x); }
  inline explicit Value(long double x) { set_double(static_cast<double>(x)); }
  inline explicit Value(const char* x) { set_string(x); }
  inline explicit Value(const std::string& x) { set_string(x); }
  inline explicit Value(const Fmi::DateTime& x) { set_ptime(x); }
  Value(const Point& x) : data(x) {}
  inline explicit Value(const BoundingBox& x) : data(x) {}
  ~Value() = default;
  Value(const Value& other) = default;

  inline bool operator==(const Value& x) const { return data == x.data; }
  inline bool operator!=(const Value& x) const { return !(data == x.data); }
  void reset();

  inline bool is_empty() const { return data.which() == TI_EMPTY; }
  inline const std::type_info& type() const
  {
    const std::type_info& ti = data.type();
    return ti == typeid(PointWrapper) ? typeid(Point) : ti;
  }

  inline void set_bool(bool value) { data = value; }
  inline void set_int(int64_t value) { data = value; }
  inline void set_uint(uint64_t value) { data = value; }
  inline void set_double(double value) { data = value; }
  inline void set_ptime(const Fmi::DateTime value) { data = value; }
  inline void set_string(const std::string& value) { data = value; }
  inline void set_point(const Point& value)
  {
    PointWrapper tmp = value;
    data = tmp;
  }
  inline void set_bbox(const BoundingBox& value) { data = value; }
  /**
   *   @brief Verifies that the value is in required ranges
   *
   *   Note that no check is done if Value object is empty, contains std::string or bool.
   *   In case of std::string no check is done even if the string could be converted to
   *   corresponding type
   */
  const Value& check_limits(
      const boost::optional<Value>& lower_limit = boost::optional<Value>(),
      const boost::optional<Value>& upper_limit = boost::optional<Value>()) const;

  inline Value& check_limits(const boost::optional<Value>& lower_limit = boost::optional<Value>(),
                             const boost::optional<Value>& upper_limit = boost::optional<Value>())
  {
    const Value* v = this;
    v->check_limits(lower_limit, upper_limit);
    return *this;
  }

  bool inside_limits(const boost::optional<Value>& lower_limit = boost::optional<Value>(),
                     const boost::optional<Value>& upper_limit = boost::optional<Value>()) const;

  bool get_bool() const;
  int64_t get_int() const;
  uint64_t get_uint() const;
  double get_double() const;
  std::string get_string() const;

  /**
   *   @brief Gets Fmi::DateTime value
   *
   *   @param use_extensions Whether to use additional parser to support
   *          strings like '12 Fmi::SecondClock ago' or 'after 3 days'.
   *
   *   The default (false) is to use just Fmi\::TimeParser
   */
  Fmi::DateTime get_ptime(bool use_extensions = false) const;

  Point get_point() const;

  BoundingBox get_bbox() const;

  inline Fmi::DateTime get_ptime_ext() const { return get_ptime(true); }
  static Value from_config(libconfig::Setting& setting);

  static Value from_config(libconfig::Config& config, const std::string& path);

  static Value from_config(libconfig::Config& config,
                           const std::string& path,
                           const Value& default_value);

  inline Value to_bool() const { return Value(get_bool()); }
  inline Value to_int() const { return Value(get_int()); }
  inline Value to_uint() const { return Value(get_uint()); }
  inline Value to_double() const { return Value(get_double()); }
  inline Value to_ptime() const { return Value(get_ptime(true)); }
  inline Value to_point() const { return Value(get_point()); }
  inline Value to_bbox() const { return Value(get_bbox()); }
  template <typename ValueType>
  ValueType get() const
  {
    get_not_implemented_for(typeid(ValueType));
  }

  template <typename ValueType>
  inline bool is() const
  {
    return typeid(ValueType) == type();
  }

  /**
   *   @brief Format as string for debug output (include actual type)
   */
  std::string dump_to_string() const;

  /**
   *   @brief Write to C++ stream in debug format (include actual type)
   */
  void print_on(std::ostream& output) const;

  /**
   *   @brief Format as string without any additional data included
   */
  std::string to_string() const;

 private:
  void bad_value_type(const std::string& location, const std::type_info& exp_type) const
      __attribute__((noreturn));

  static void bad_config_value(const std::string& location, const std::string& type);

  void get_not_implemented_for(const std::type_info& type) const __attribute__((noreturn));
};

inline std::ostream& operator<<(std::ostream& ost, const Value& value)
{
  value.print_on(ost);
  return ost;
}

template <>
bool Value::get() const;
template <>
int64_t Value::get() const;
template <>
uint64_t Value::get() const;
template <>
double Value::get() const;
template <>
std::string Value::get() const;
template <>
Fmi::DateTime Value::get() const;
template <>
Point Value::get() const;
template <>
BoundingBox Value::get() const;
template <>
Value Value::get() const;

}  // namespace Spine
}  // namespace SmartMet
