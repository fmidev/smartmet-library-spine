// ======================================================================
/*!
 * \file
 * \brief Regression tests for functions in SmartMet::Spine::FmiApiKey namespace.
 */
// ======================================================================

#include "FmiApiKey.h"
#include <boost/test/included/unit_test.hpp>

using namespace boost::unit_test;
using namespace SmartMet::Spine;

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  const char* name = "FmiApiKey tester";
  unit_test_log.set_threshold_level(log_messages);
  framework::master_test_suite().p_name.value = name;
  BOOST_TEST_MESSAGE("");
  BOOST_TEST_MESSAGE(name);
  BOOST_TEST_MESSAGE(std::string(std::strlen(name), '='));
  return NULL;
}

BOOST_AUTO_TEST_CASE(test_getFmiApiKey_keyInHeader)
{
  const std::string apikey{"aa-bb-cc-dd-ee-ff"};
  const HTTP::HeaderMap headers = {{"fmi-apikey", apikey}};
  const std::string accessToken{"00-11-22-33-44-55"};
  const HTTP::ParamMap params = {{"fmi-apikey", apikey}, {"access-token", accessToken}};
  const HTTP::Request request{headers, {}, "1.1", params, "/", HTTP::RequestMethod::GET, false};
  const auto key{FmiApiKey::getFmiApiKey(request)};
  BOOST_TEST_REQUIRE(key.has_value());
  BOOST_TEST(*key == apikey);
}

BOOST_AUTO_TEST_CASE(test_getFmiApiKey_keyInParameter)
{
  const std::string apikey{"aa-bb-cc-dd-ee-ff"};
  const std::string accessToken{"00-11-22-33-44-55"};
  const HTTP::ParamMap params = {{"fmi-apikey", apikey}, {"access-token", accessToken}};
  const HTTP::Request request{{}, {}, "1.1", params, "/", HTTP::RequestMethod::GET, false};
  const auto key{FmiApiKey::getFmiApiKey(request)};
  BOOST_TEST_REQUIRE(key.has_value());
  BOOST_TEST(*key == apikey);
}

BOOST_AUTO_TEST_CASE(test_getFmiApiKey_accessToken)
{
  const std::string accessToken{"00-11-22-33-44-55"};
  const HTTP::ParamMap params = {{"access-token", accessToken}};
  const HTTP::Request request{{}, {}, "1.1", params, "/", HTTP::RequestMethod::GET, false};
  auto token{FmiApiKey::getFmiApiKey(request)};
  BOOST_TEST(token.has_value() == false);
  token = {FmiApiKey::getFmiApiKey(request, true)};
  BOOST_TEST_REQUIRE(token.has_value());
  BOOST_TEST(*token == accessToken);
}

BOOST_AUTO_TEST_CASE(test_shouldReturnApiKey_noOmitHeader)
{
  const HTTP::Request request{{}, {}, "1.1", {}, "/", HTTP::RequestMethod::GET, false};
  BOOST_TEST(FmiApiKey::shouldReturnApiKey(request) == true);
}

BOOST_AUTO_TEST_CASE(test_shouldReturnApiKey_omitHeaderZero)
{
  const HTTP::HeaderMap headers = {{"omit-fmi-apikey", "0"}};
  const HTTP::Request request{headers, {}, "1.1", {}, "/", HTTP::RequestMethod::GET, false};
  BOOST_TEST(FmiApiKey::shouldReturnApiKey(request) == true);
}

BOOST_AUTO_TEST_CASE(test_shouldReturnApiKey_omitHeaderNonZero)
{
  const HTTP::HeaderMap headers = {{"omit-fmi-apikey", "1"}};
  const HTTP::Request request{headers, {}, "1.1", {}, "/", HTTP::RequestMethod::GET, false};
  BOOST_TEST(FmiApiKey::shouldReturnApiKey(request) == false);
}

BOOST_AUTO_TEST_CASE(test_shouldReturnApiKey_omitHeaderFalse)
{
  const HTTP::HeaderMap headers = {{"omit-fmi-apikey", "false"}};
  const HTTP::Request request{headers, {}, "1.1", {}, "/", HTTP::RequestMethod::GET, false};
  BOOST_TEST(FmiApiKey::shouldReturnApiKey(request) == true);
}
