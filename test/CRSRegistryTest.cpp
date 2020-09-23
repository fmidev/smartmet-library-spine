// FIXME: move to smartmet-engine-gis
#include <iostream>
#include <boost/test/included/unit_test.hpp>
#include <newbase/NFmiPoint.h>
#include "CRSRegistry.h"

using namespace boost::unit_test;
using namespace SmartMet::Spine;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "CRSRegistry tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(test_coordinate_transformation_1)
{
  // namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE("+ [Coordinate system registry class]");

  CRSRegistry registry;
  registry.register_epsg("WGS84", 4326);
  registry.register_epsg("UTM-ZONE-35", 4037);
  registry.register_epsg("WGS72-UTM-35", 32235);

  boost::shared_ptr<CRSRegistry::Transformation> conv, conv2;

  BOOST_REQUIRE_NO_THROW(conv = registry.create_transformation("WGS84", "UTM-ZONE-35"));
  NFmiPoint p1(24.0, 60.0), p2, p3;
  BOOST_REQUIRE_NO_THROW(p2 = conv->transform(p1));

  BOOST_CHECK_SMALL(332705.1789 - p2.X(), 0.001);
  BOOST_CHECK_SMALL(6655205.4836 - p2.Y(), 0.001);

  BOOST_REQUIRE_NO_THROW(conv2 = registry.create_transformation("UTM-ZONE-35", "WGS84"));
  BOOST_REQUIRE_NO_THROW(p3 = conv2->transform(p2));

  BOOST_CHECK_SMALL(24.0 - p3.X(), 1e-8);
  BOOST_CHECK_SMALL(60.0 - p3.Y(), 1e-8);

  boost::array<double, 3> x1, x2;
  x1[0] = 24.0;
  x1[1] = 60.0;
  x1[2] = 10.0;
  BOOST_REQUIRE_NO_THROW(x2 = conv->transform(x1));

  BOOST_CHECK_SMALL(332705.1789 - x2[0], 0.001);
  BOOST_CHECK_SMALL(6655205.4836 - x2[1], 0.001);
  BOOST_CHECK_SMALL(10.0 - x2[2], 0.001);

  BOOST_REQUIRE_NO_THROW(conv = registry.create_transformation("WGS84", "WGS72-UTM-35"));
  BOOST_REQUIRE_NO_THROW(p2 = conv->transform(p1));
  BOOST_CHECK_SMALL(332696.5477 - p2.X(), 0.001);
  BOOST_CHECK_SMALL(6655201.5978 - p2.Y(), 0.001);
}

BOOST_AUTO_TEST_CASE(test_coordinate_transformation_2)
{
  // namespace pt = boost::posix_time;

  BOOST_TEST_MESSAGE("+ [Coordinate system registry class] conversion (swap coordinates)");

  CRSRegistry registry;
  registry.register_epsg("WGS84", 4326, boost::optional<std::string>(), true);
  registry.register_epsg("WGS84B", 4326, boost::optional<std::string>(), false);
  registry.register_epsg("UTM-ZONE-35", 4037);
  registry.register_epsg("WGS72-UTM-35", 32235);

  boost::shared_ptr<CRSRegistry::Transformation> conv, conv2;

  BOOST_REQUIRE_NO_THROW(conv = registry.create_transformation("WGS84", "UTM-ZONE-35"));
  NFmiPoint p1(60.0, 24.0), p2, p3;
  BOOST_REQUIRE_NO_THROW(p2 = conv->transform(p1));

  BOOST_CHECK_SMALL(332705.1789 - p2.X(), 0.001);
  BOOST_CHECK_SMALL(6655205.4836 - p2.Y(), 0.001);

  BOOST_REQUIRE_NO_THROW(conv2 = registry.create_transformation("UTM-ZONE-35", "WGS84B"));
  BOOST_REQUIRE_NO_THROW(p3 = conv2->transform(p2));

  BOOST_CHECK_SMALL(24.0 - p3.X(), 1e-8);
  BOOST_CHECK_SMALL(60.0 - p3.Y(), 1e-8);

  boost::array<double, 3> x1, x2;
  x1[0] = 60.0;
  x1[1] = 24.0;
  x1[2] = 10.0;
  BOOST_REQUIRE_NO_THROW(x2 = conv->transform(x1));

  BOOST_CHECK_SMALL(332705.1789 - x2[0], 0.001);
  BOOST_CHECK_SMALL(6655205.4836 - x2[1], 0.001);
  BOOST_CHECK_SMALL(10.0 - x2[2], 0.001);

  BOOST_REQUIRE_NO_THROW(conv = registry.create_transformation("WGS84", "WGS72-UTM-35"));
  BOOST_REQUIRE_NO_THROW(p2 = conv->transform(p1));
  BOOST_CHECK_SMALL(332696.5477 - p2.X(), 0.001);
  BOOST_CHECK_SMALL(6655201.5978 - p2.Y(), 0.001);
}

BOOST_AUTO_TEST_CASE(test_finding_ccordinates_using_regex)
{
  BOOST_TEST_MESSAGE("+ [Finding CRS using REGEX]");

  CRSRegistry registry;
  registry.register_epsg("WGS84", 4326, std::string("(urn:ogc:def:crs:|)EPSG:{1,2}4326"));
  registry.register_epsg("UTM-ZONE-35", 4037, std::string("(urn:ogc:def:crs:|)EPSG:{1,2}4037"));
  registry.register_epsg("WGS72-UTM-35", 32235, std::string("(urn:ogc:def:crs:|)EPSG:{1,2}32235"));

  std::string s11, s12, s13, s21, s22, s22a, s23;

  BOOST_REQUIRE_NO_THROW(s11 = registry.get_proj4("WGS84"));
  BOOST_REQUIRE_NO_THROW(s12 = registry.get_proj4("UTM-ZONE-35"));
  BOOST_REQUIRE_NO_THROW(s13 = registry.get_proj4("WGS72-UTM-35"));

  BOOST_REQUIRE_NO_THROW(s21 = registry.get_proj4("urn:ogc:def:crs:EPSG::4326"));
  BOOST_REQUIRE_NO_THROW(s22 = registry.get_proj4("EPSG::4037"));
  BOOST_REQUIRE_NO_THROW(s22a = registry.get_proj4("EPSG:4037"));
  BOOST_REQUIRE_NO_THROW(s23 = registry.get_proj4("urn:ogc:def:crs:EPSG::32235"));

  BOOST_CHECK_EQUAL(s11, s21);
  BOOST_CHECK_EQUAL(s12, s22);
  BOOST_CHECK_EQUAL(s12, s22a);
  BOOST_CHECK_EQUAL(s13, s23);
}

BOOST_AUTO_TEST_CASE(test_crs_attrib)
{
  BOOST_TEST_MESSAGE("+ [Test attaching attributes to CRS]");

  CRSRegistry registry;
  registry.register_epsg("WGS84", 4326, std::string("(urn:ogc:def:crs:|)EPSG::4326"));
  registry.register_epsg("UTM-ZONE-35", 4037, std::string("(urn:ogc:def:crs:|)EPSG::4037"));
  registry.register_epsg("WGS72-UTM-35", 32235, std::string("(urn:ogc:def:crs:|)EPSG::32235"));

  bool found = true;
  int i1;
  double foo = 1.23, bar = 0;
  BOOST_REQUIRE_NO_THROW(registry.set_attribute("EPSG::4037", "foo", foo));
  BOOST_REQUIRE_THROW(registry.get_attribute("EPSG::4307", "foo", &i1), Fmi::Exception);
  BOOST_REQUIRE_NO_THROW(found = registry.get_attribute("EPSG::4326", "foo", &bar));
  BOOST_CHECK(not found);
  BOOST_REQUIRE_NO_THROW(found = registry.get_attribute("EPSG::4037", "foo", &bar));
  BOOST_CHECK(found);
  BOOST_CHECK_CLOSE(foo, bar, 1e-10);
}
