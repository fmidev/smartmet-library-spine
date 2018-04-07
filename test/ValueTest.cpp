#include "Exception.h"
#include "Value.h"
#include <boost/test/included/unit_test.hpp>
#include <macgyver/StringConversion.h>
#include <iostream>

using namespace boost::unit_test;
using SmartMet::Spine::Value;
namespace pt = boost::posix_time;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "Value tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_SUITE(brainstorm_value_tests)

BOOST_AUTO_TEST_CASE(test_boolean_value)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing boolean value");

  Value v1;
  bool t1 = true;

  v1.set_bool(false);
  BOOST_CHECK_NO_THROW(t1 = v1.get_bool());
  BOOST_CHECK(not t1);
  BOOST_CHECK_EQUAL(std::string("false"), v1.to_string());
  BOOST_CHECK(not v1.get<bool>());

  v1.set_bool(true);
  BOOST_CHECK_NO_THROW(t1 = v1.get_bool());
  BOOST_CHECK(t1);
  BOOST_CHECK_EQUAL(std::string("true"), v1.to_string());

  BOOST_CHECK_THROW(v1.get_int(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_uint(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_double(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_ptime(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_string(), SmartMet::Spine::Exception);

  BOOST_CHECK(v1.get<bool>());
}

BOOST_AUTO_TEST_CASE(test_int_value)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing integer value");

  Value v1;
  int64_t i1 = 0;
  bool b1 = false;
  uint64_t u1 = 0;
  double d1;

  v1.set_int(345);
  BOOST_CHECK_NO_THROW(b1 = v1.get_bool());
  BOOST_CHECK(b1);
  BOOST_CHECK_NO_THROW(i1 = v1.get_int());
  BOOST_CHECK_EQUAL((int)i1, 345);
  BOOST_CHECK_NO_THROW(u1 = v1.get_uint());
  BOOST_CHECK_EQUAL((int)u1, 345);
  BOOST_CHECK_NO_THROW(d1 = v1.get_double());
  BOOST_CHECK_CLOSE(d1, 345, 1e-12);
  BOOST_CHECK_THROW(v1.get_ptime(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_string(), SmartMet::Spine::Exception);
  BOOST_CHECK_EQUAL(std::string("345"), v1.to_string());
  BOOST_CHECK_EQUAL(345, (int)v1.get<int64_t>());

  v1.set_int(0);
  BOOST_CHECK_NO_THROW(b1 = v1.get_bool());
  BOOST_CHECK(not b1);
  BOOST_CHECK_NO_THROW(i1 = v1.get_int());
  BOOST_CHECK_EQUAL((int)i1, 0);
  BOOST_CHECK_NO_THROW(u1 = v1.get_uint());
  BOOST_CHECK_EQUAL((int)u1, 0);
  BOOST_CHECK_NO_THROW(d1 = v1.get_double());
  BOOST_CHECK_CLOSE(d1, 0.0, 1e-12);
  BOOST_CHECK_EQUAL(std::string("0"), v1.to_string());

  v1.set_int(-963);
  BOOST_CHECK_NO_THROW(b1 = v1.get_bool());
  BOOST_CHECK(b1);
  BOOST_CHECK_NO_THROW(i1 = v1.get_int());
  BOOST_CHECK_EQUAL((int)i1, -963);
  BOOST_CHECK_THROW(u1 = v1.get_uint(), SmartMet::Spine::Exception);
  BOOST_CHECK_NO_THROW(d1 = v1.get_double());
  BOOST_CHECK_CLOSE(d1, -963, 1e-12);
  BOOST_CHECK_EQUAL(std::string("-963"), v1.to_string());
}

BOOST_AUTO_TEST_CASE(test_uint_value)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing unsigned integer value");

  Value v1, v2, v3;
  int64_t i1 = -1;
  bool b1 = false;
  uint64_t u1;
  double d1;

  v1.set_uint(345);
  BOOST_CHECK_NO_THROW(b1 = v1.get_bool());
  BOOST_CHECK(b1);
  BOOST_CHECK_NO_THROW(i1 = v1.get_int());
  BOOST_CHECK_EQUAL((int)i1, 345);
  BOOST_CHECK_NO_THROW(u1 = v1.get_uint());
  BOOST_CHECK_EQUAL((int)u1, 345);
  BOOST_CHECK_NO_THROW(d1 = v1.get_double());
  BOOST_CHECK_CLOSE(d1, 345, 1e-12);
  BOOST_CHECK_THROW(v1.get_ptime(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_string(), SmartMet::Spine::Exception);
  BOOST_CHECK_EQUAL(std::string("345"), v1.to_string());
  BOOST_CHECK_EQUAL(345, (int)v1.get<uint64_t>());

  v2.set_uint(0);
  BOOST_CHECK_NO_THROW(b1 = v2.get_bool());
  BOOST_CHECK(not b1);
  BOOST_CHECK_NO_THROW(i1 = v2.get_int());
  BOOST_CHECK_EQUAL((int)i1, 0);
  BOOST_CHECK_NO_THROW(u1 = v2.get_uint());
  BOOST_CHECK_EQUAL((int)u1, 0);
  BOOST_CHECK_NO_THROW(d1 = v2.get_double());
  BOOST_CHECK_CLOSE(d1, 0.0, 1e-12);
  BOOST_CHECK_EQUAL(std::string("0"), v2.to_string());

  const uint64_t x1 = 0xFFFFFFFF00000000LLU;
  v3.set_uint(x1);
  BOOST_CHECK_NO_THROW(b1 = v3.get_bool());
  BOOST_CHECK(b1);
  BOOST_CHECK_THROW(v3.get_int(), SmartMet::Spine::Exception);
  BOOST_CHECK_NO_THROW(u1 = v3.get_uint());
  BOOST_CHECK_EQUAL(u1, x1);
  BOOST_CHECK_NO_THROW(d1 = v3.get_double());
}

BOOST_AUTO_TEST_CASE(test_double_value)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing double value");

  Value v1;
  const double d0 = 234.56;
  double d1;

  v1.set_double(234.56);
  BOOST_CHECK_THROW(v1.get_bool(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_int(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_uint(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_ptime(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_string(), SmartMet::Spine::Exception);
  BOOST_CHECK_NO_THROW(d1 = v1.get_double());
  BOOST_CHECK_CLOSE(d1, d0, 1e-10);
  BOOST_CHECK_CLOSE(234.56, v1.get<double>(), 1e-10);

  v1.set_double(M_PI);
  BOOST_CHECK_EQUAL(std::string("3.141592653589"), v1.to_string().substr(0, 14));
}

BOOST_AUTO_TEST_CASE(test_ptime_value)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing boost::posix_time::ptime value");

  Value v1;
  const pt::ptime now = pt::second_clock::universal_time();
  pt::ptime t1;
  v1.set_ptime(now);
  BOOST_CHECK_THROW(v1.get_bool(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_int(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_uint(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_double(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v1.get_string(), SmartMet::Spine::Exception);
  BOOST_CHECK_NO_THROW(t1 = v1.get_ptime());
  BOOST_CHECK(t1 == now);
  BOOST_CHECK(now == v1.get<pt::ptime>());

  v1.set_ptime(pt::time_from_string("2012-10-29 17:20:19"));
  BOOST_CHECK_EQUAL(std::string("20121029T172019Z"), v1.to_string());
}

BOOST_AUTO_TEST_CASE(test_string_values)
{
  const pt::ptime now = pt::second_clock::universal_time();

  BOOST_TEST_MESSAGE("+ [Value] Testing string value");

  int64_t i1;
  uint64_t u1;
  double d1;
  std::string s1;
  pt::ptime t1;

  Value v1;
  v1.set_string("123");
  BOOST_CHECK_NO_THROW(i1 = v1.get_int());
  BOOST_CHECK_EQUAL(i1, 123);
  BOOST_CHECK_NO_THROW(u1 = v1.get_uint());
  BOOST_CHECK_EQUAL(u1, 123U);
  BOOST_CHECK_NO_THROW(d1 = v1.get_double());
  BOOST_CHECK_CLOSE(d1, 123.0, 1e-10);
  BOOST_CHECK_NO_THROW(s1 = v1.get_string());
  BOOST_CHECK_EQUAL(s1, std::string("123"));
  BOOST_CHECK_EQUAL(std::string("123"), v1.get<std::string>());
  // BOOST_CHECK_THROW(v1.get_ptime(), SmartMet::Spine::Exception);

  Value v2;
  v2.set_string("345.67");
  BOOST_CHECK_THROW(v2.get_int(), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(v2.get_uint(), SmartMet::Spine::Exception);
  BOOST_CHECK_NO_THROW(d1 = v2.get_double());
  BOOST_CHECK_CLOSE(d1, 345.67, 1e-10);
  BOOST_CHECK_THROW(v2.get_ptime(), SmartMet::Spine::Exception);

  Value v3;
  v3.set_string(Fmi::to_iso_string(now));
  BOOST_CHECK_NO_THROW(t1 = v3.get_ptime());
  BOOST_CHECK(t1 == now);
}

BOOST_AUTO_TEST_CASE(test_point_value)
{
  using SmartMet::Spine::Point;

  BOOST_TEST_MESSAGE("+ [Value] Testing parsing SmartMet::Spine::Point from string");

  Point p, p2;

  BOOST_REQUIRE_NO_THROW(p.parse_string("24.0,61.0"));
  BOOST_CHECK_CLOSE(p.x, 24.0, 1e-10);
  BOOST_CHECK_CLOSE(p.y, 61.0, 1e-10);
  BOOST_CHECK_EQUAL(p.crs, "");

  BOOST_REQUIRE_NO_THROW(p.parse_string("25.0,66.1,EPSG:4258"));
  BOOST_CHECK_CLOSE(p.x, 25.0, 1e-10);
  BOOST_CHECK_CLOSE(p.y, 66.1, 1e-10);
  BOOST_CHECK_EQUAL(p.crs, "EPSG:4258");

  Point p3 = p;
  BOOST_CHECK(p == p);
  BOOST_CHECK(p == p3);

  Value value(p);
  BOOST_REQUIRE_NO_THROW(p2 = value.get_point());
  BOOST_REQUIRE_NO_THROW(value.dump_to_string());
  BOOST_REQUIRE_NO_THROW(value.to_string());
  BOOST_CHECK(p == p2);
}

BOOST_AUTO_TEST_CASE(test_bounding_box_value)
{
  using SmartMet::Spine::BoundingBox;

  BOOST_TEST_MESSAGE("+ [Value] Testing parsing SmartMet::Spine::BoundingBox from string");

  BoundingBox b, b2;

  BOOST_REQUIRE_NO_THROW(b.parse_string("24.0,61.0,25.0,62.0"));
  BOOST_CHECK_CLOSE(b.xMin, 24.0, 1e-10);
  BOOST_CHECK_CLOSE(b.yMin, 61.0, 1e-10);
  BOOST_CHECK_CLOSE(b.xMax, 25.0, 1e-10);
  BOOST_CHECK_CLOSE(b.yMax, 62.0, 1e-10);
  BOOST_CHECK_EQUAL(b.crs, "");

  BOOST_REQUIRE_NO_THROW(b.parse_string("25.0,66.1,25.5,66.6,EPSG:4258"));
  BOOST_CHECK_CLOSE(b.xMin, 25.0, 1e-10);
  BOOST_CHECK_CLOSE(b.yMin, 66.1, 1e-10);
  BOOST_CHECK_CLOSE(b.xMax, 25.5, 1e-10);
  BOOST_CHECK_CLOSE(b.yMax, 66.6, 1e-10);
  BOOST_CHECK_EQUAL(b.crs, "EPSG:4258");

  BoundingBox b3 = b;
  BOOST_CHECK(b == b);
  BOOST_CHECK(b == b3);

  Value value(b);
  BOOST_REQUIRE_NO_THROW(b2 = value.get_bbox());
  BOOST_REQUIRE_NO_THROW(value.dump_to_string());
  BOOST_REQUIRE_NO_THROW(value.to_string());
  BOOST_CHECK(b2 == b);
}

BOOST_AUTO_TEST_CASE(test_config_input)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing reading value from configuration file");

  libconfig::Config config;
  BOOST_REQUIRE_NO_THROW(config.getRoot().add("a", libconfig::Setting::TypeInt) = 12);
  BOOST_REQUIRE_NO_THROW(config.getRoot().add("b", libconfig::Setting::TypeInt64) = -2LL);
  BOOST_REQUIRE_NO_THROW(config.getRoot().add("c", libconfig::Setting::TypeFloat) = 234.56);
  BOOST_REQUIRE_NO_THROW(config.getRoot().add("d", libconfig::Setting::TypeString) = "foobar");
  BOOST_REQUIRE_NO_THROW(config.getRoot().add("e", libconfig::Setting::TypeString) =
                             "2012-10-29T14:45:47Z");
  BOOST_REQUIRE_NO_THROW(config.getRoot().add("f", libconfig::Setting::TypeString) =
                             "25.0,66.1,25.5,66.6,EPSG:4258");

  int64_t i1;
  double d1;
  pt::ptime t1;
  SmartMet::Spine::BoundingBox b1;

  BOOST_CHECK_NO_THROW(i1 = Value::from_config(config, "a").get_int());
  BOOST_CHECK_EQUAL(i1, 12);
  BOOST_CHECK_NO_THROW(d1 = Value::from_config(config, "c").get_double());
  BOOST_CHECK_CLOSE(d1, 234.56, 1e-10);
  BOOST_CHECK_NO_THROW(t1 = Value::from_config(config, "e").get_ptime());

  BOOST_CHECK_NO_THROW(b1 = Value::from_config(config, "f").get_bbox());
  BOOST_CHECK_CLOSE(b1.xMin, 25.0, 1e-10);
  BOOST_CHECK_CLOSE(b1.yMin, 66.1, 1e-10);
  BOOST_CHECK_CLOSE(b1.xMax, 25.5, 1e-10);
  BOOST_CHECK_CLOSE(b1.yMax, 66.6, 1e-10);
  BOOST_CHECK_EQUAL(b1.crs, "EPSG:4258");
}

BOOST_AUTO_TEST_CASE(test_checking_int_limits)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing range check of integer value");

  const Value lower_limit(100);
  const Value upper_limit(222);

  BOOST_CHECK_THROW(Value(1).check_limits(lower_limit, upper_limit), SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(Value(99).check_limits(lower_limit, upper_limit), SmartMet::Spine::Exception);
  BOOST_CHECK_NO_THROW(Value(100).check_limits(lower_limit, upper_limit));
  BOOST_CHECK_NO_THROW(Value(120).check_limits(lower_limit, upper_limit));
  BOOST_CHECK_NO_THROW(Value(222).check_limits(lower_limit, upper_limit));
  BOOST_CHECK_THROW(Value(223).check_limits(lower_limit, upper_limit), SmartMet::Spine::Exception);
}

BOOST_AUTO_TEST_CASE(test_checking_ptime_limits)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing range check of boost::posix_time::ptime value");

  const Value lower_limit(pt::time_from_string("2013-01-01 00:00:00"));
  boost::optional<Value> upper_limit;

  BOOST_CHECK_THROW(
      Value(pt::time_from_string("2011-03-04 12:13:13")).check_limits(lower_limit, upper_limit),
      SmartMet::Spine::Exception);
  BOOST_CHECK_THROW(
      Value(pt::time_from_string("2012-12-31 23:59:59")).check_limits(lower_limit, upper_limit),
      SmartMet::Spine::Exception);
  BOOST_CHECK_NO_THROW(
      Value(pt::time_from_string("2013-01-01 00:00:00")).check_limits(lower_limit, upper_limit));
  BOOST_CHECK_NO_THROW(
      Value(pt::time_from_string("2014-02-03 11:23:21")).check_limits(lower_limit, upper_limit));

  const Value tmp(pt::time_from_string("2014-02-03 11:23:21"));
  BOOST_CHECK_NO_THROW(tmp.check_limits(lower_limit, upper_limit));
}

BOOST_AUTO_TEST_CASE(test_checking_ptime_limits_2)
{
  BOOST_TEST_MESSAGE("+ [Value] Testing range check of boost::posix_time::ptime value");

  const Value lower_limit(pt::time_from_string("2013-01-01 00:00:00"));
  boost::optional<Value> upper_limit;

  BOOST_CHECK(not Value(pt::time_from_string("2011-03-04 12:13:13"))
                      .inside_limits(lower_limit, upper_limit));
  BOOST_CHECK(not Value(pt::time_from_string("2012-12-31 23:59:59"))
                      .inside_limits(lower_limit, upper_limit));
  BOOST_CHECK(
      Value(pt::time_from_string("2013-01-01 00:00:00")).inside_limits(lower_limit, upper_limit));
  BOOST_CHECK(
      Value(pt::time_from_string("2014-02-03 11:23:21")).inside_limits(lower_limit, upper_limit));
}

BOOST_AUTO_TEST_CASE(test_checking_value_cast)
{
  BOOST_TEST_MESSAGE("+ [Value] Test casting value content");

  const Value v1("1");
  auto v1a = v1.to_int();
  BOOST_CHECK(typeid(int64_t) == v1a.type());
  BOOST_CHECK_EQUAL(1, (int)v1a.get_int());

  const Value v2("2013-01-01 00:00:00");
  auto v2a = v2.to_ptime();
  BOOST_CHECK(typeid(pt::ptime) == v2a.type());
  BOOST_CHECK_EQUAL(pt::time_from_string("2013-01-01 00:00:00"), v2a.get_ptime());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(string2ptime_tests)

//*************************************************
//
// Tests for string2ptime
//
//*************************************************

BOOST_AUTO_TEST_CASE(test_parsing_relative_time_1)
{
  using namespace SmartMet::Spine;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE("+ [string2ptime()] Testing parsing relative time: value 'now'");

  pt::ptime t_ref = pt::time_from_string("2012-10-29 17:20:19");
  pt::ptime t1;
  pt::time_duration dt;

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("now", t_ref));
  dt = t_ref - t1;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 0);
}

BOOST_AUTO_TEST_CASE(test_parsing_relative_time_2)
{
  using namespace SmartMet::Spine;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE(
      "+ [string2ptime()] Testing parsing relative time: values 'n seconds ago' and 'after n "
      "seconds'");

  pt::ptime t_ref = pt::time_from_string("2012-10-29 17:20:19");
  pt::ptime t1;
  pt::time_duration dt;

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("123 seconds ago", t_ref));
  dt = t_ref - t1;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 123);

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("after 123 seconds", t_ref));
  dt = t1 - t_ref;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 123);
}

BOOST_AUTO_TEST_CASE(test_parsing_relative_time_3)
{
  using namespace SmartMet::Spine;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE(
      "+ [string2ptime()] Testing parsing relative time: values 'n minutes ago' and 'after n "
      "minutes'");

  pt::ptime t_ref = pt::time_from_string("2012-10-29 17:20:19");
  pt::ptime t1;
  pt::time_duration dt;

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("64 minutes ago", t_ref));
  dt = t_ref - t1;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 64 * 60);

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("after 64 minutes", t_ref));
  dt = t1 - t_ref;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 64 * 60);
}

BOOST_AUTO_TEST_CASE(test_parsing_relative_time_4)
{
  using namespace SmartMet::Spine;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE(
      "+ [string2ptime()] Testing parsing relative time: values 'n hours ago' and 'after n hours'");

  pt::ptime t_ref = pt::time_from_string("2012-10-29 17:20:19");
  pt::ptime t1;
  pt::time_duration dt;

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("12 hours ago", t_ref));
  dt = t_ref - t1;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 12 * 3600);

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("after 12 hours", t_ref));
  dt = t1 - t_ref;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 12 * 3600);
}

BOOST_AUTO_TEST_CASE(test_parsing_relative_time_5)
{
  using namespace SmartMet::Spine;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE(
      "+ [string2ptime()] Testing parsing relative time: values 'n days ago' and 'after n days'");

  pt::ptime t_ref = pt::time_from_string("2012-10-29 17:20:19");
  pt::ptime t1;
  pt::time_duration dt;

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("3 days ago", t_ref));
  dt = t_ref - t1;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 3 * 86400);

  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("after 3 days", t_ref));
  dt = t1 - t_ref;
  BOOST_CHECK_EQUAL(dt.total_seconds(), 3 * 86400);
}

BOOST_AUTO_TEST_CASE(test_parsing_relative_time_with_rounding)
{
  using namespace SmartMet::Spine;
  namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE("+ [string2ptime()] Testing parsing relative time with rounding");

  pt::ptime t_ref = pt::time_from_string("2012-10-29 17:22:19");

  pt::ptime t1;
  pt::ptime t1_exp = pt::time_from_string("2012-10-29 15:20:00");
  BOOST_REQUIRE_NO_THROW(t1 = string2ptime("2 hours ago rounded 5 min", t_ref));
  BOOST_CHECK_EQUAL(t1_exp, t1);

  pt::ptime t2;
  pt::ptime t2_exp = pt::time_from_string("2012-10-29 18:15:00");
  BOOST_REQUIRE_NO_THROW(t2 = string2ptime("after 1 hour rounded 15 minutes", t_ref));
  BOOST_CHECK_EQUAL(t2_exp, t2);

  pt::ptime t3;
  pt::ptime t3_exp = pt::time_from_string("2012-10-29 17:00:00");
  BOOST_REQUIRE_NO_THROW(t3 = string2ptime("now rounded 60 min", t_ref));
  BOOST_CHECK_EQUAL(t3_exp, t3);

  pt::ptime t4;
  pt::ptime t4_exp = pt::time_from_string("2012-10-29 15:25:00");
  BOOST_REQUIRE_NO_THROW(t4 = string2ptime("2 hours ago rounded up 5 min", t_ref));
  BOOST_CHECK_EQUAL(t4_exp, t4);

  pt::ptime t5;
  pt::ptime t_ref5 = pt::time_from_string("2012-10-29 22:56:17");
  pt::ptime t5_exp = pt::time_from_string("2012-10-30 00:00:00");
  BOOST_REQUIRE_NO_THROW(t5 = string2ptime("after 1 hour rounded up 5 min", t_ref5));
  BOOST_CHECK_EQUAL(t5_exp, t5);
}

BOOST_AUTO_TEST_CASE(test_parse_xml_time)
{
  using namespace SmartMet::Spine;
  namespace pt = boost::posix_time;

  pt::ptime t1;

  BOOST_CHECK_NO_THROW(t1 = parse_xml_time("2013-05-15T12:59:12Z"));
  BOOST_CHECK_EQUAL(t1, pt::time_from_string("2013-05-15 12:59:12"));

  BOOST_CHECK_NO_THROW(t1 = parse_xml_time("2013-05-15T12:59:12+03:00"));
  BOOST_CHECK_EQUAL(t1, pt::time_from_string("2013-05-15 09:59:12"));

  BOOST_CHECK_NO_THROW(t1 = parse_xml_time("2013-05-15T12:59:12-03:01"));
  BOOST_CHECK_EQUAL(t1, pt::time_from_string("2013-05-15 16:00:12"));

  // We suuport leaving leaving seconds out (in the real format they are mandatory)
  BOOST_CHECK_NO_THROW(t1 = parse_xml_time("2013-05-15T13:05Z"));
  BOOST_CHECK_EQUAL(t1, pt::time_from_string("2013-05-15 13:05:00"));

  BOOST_CHECK_NO_THROW(t1 = parse_xml_time("2013-05-15T13:05+03:00"));
  BOOST_CHECK_EQUAL(t1, pt::time_from_string("2013-05-15 10:05:00"));

  Value test("2013-05-15T12:59:12+03:00");
  BOOST_CHECK_NO_THROW(t1 = test.get_ptime(true));
  BOOST_CHECK_EQUAL(t1, pt::time_from_string("2013-05-15 09:59:12"));

  BOOST_CHECK_NO_THROW(t1 = test.get_ptime(false));
  BOOST_CHECK_EQUAL(t1, pt::time_from_string("2013-05-15 09:59:12"));

  // Test fall-back to Fmi::TimeParser
  test.set_string("20130515131723");
  BOOST_CHECK_NO_THROW(t1 = test.get_ptime(false));
  BOOST_CHECK_EQUAL(t1, pt::time_from_string("2013-05-15 13:17:23"));
}

BOOST_AUTO_TEST_SUITE_END()
