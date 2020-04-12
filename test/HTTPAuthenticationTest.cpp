#include "HTTPAuthentication.h"
#include <macgyver/Base64.h>
#include <regression/tframe.h>
#include <iostream>
#include <string>

using namespace SmartMet::Spine;

namespace HTTPTest
{
namespace
{
std::string makeBasicAuthHash(const std::string& user, const std::string& password)
{
  return "Basic " + Fmi::Base64::encode(user + ":" + password);
}

class AuthenticationExtTest : public HTTP::Authentication
{
 public:
  AuthenticationExtTest() : HTTP::Authentication(true) {}

 protected:
  unsigned getAuthenticationGroup(const HTTP::Request& request) const override
  {
    if (request.getResource() == "/foo")
    {
      return 2;
    }
    else
    {
      return 4;
    }
  }
};
}  // namespace

void auth_missing()
{
  HTTP::HeaderMap headers = {{"Content-Type", "text/html; charset=\"UTF-8\""}};
  HTTP::ParamMap params = {{"foo", "bar"}};
  HTTP::Request request(headers, "", "1.1", params, "/foo", HTTP::RequestMethod::GET, false);
  HTTP::Authentication auth(true);
  auth.addUser("user", "password");
  HTTP::Response response;
  if (auth.authenticateRequest(request, response))
  {
    TEST_FAILED("Auth missing: authentication should not pass");
  }
  else if (response.getStatus() == 401)
  {
    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Incorrect return code of failed authentication");
  }
}

void correct_auth_provided()
{
  HTTP::HeaderMap headers = {{"Content-Type", "text/html; charset=\"UTF-8\""},
                             {"Authorization", makeBasicAuthHash("user_b", "password_b")}};
  HTTP::ParamMap params = {{"foo", "bar"}};
  HTTP::Request request(headers, "", "1.1", params, "/foo", HTTP::RequestMethod::GET, false);
  HTTP::Authentication auth(true);
  auth.addUser("user_a", "password_a");
  auth.addUser("user_b", "password_b");
  HTTP::Response response;
  if (auth.authenticateRequest(request, response))
  {
    TEST_PASSED();
  }
  else
  {
    std::cout << "Request:\n" << request.toString() << std::endl;
    std::cout << "Response:\n" << response.toString() << std::endl;
    TEST_FAILED("Correct authentication rejected");
  }

  if (auth.removeUser("user_a"))
  {
    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Removing existing user returned false");
  }

  if (auth.authenticateRequest(request, response))
  {
    TEST_PASSED();
  }
  else
  {
    std::cout << "Request:\n" << request.toString() << std::endl;
    std::cout << "Response:\n" << response.toString() << std::endl;
    TEST_FAILED("Correct authentication rejected");
  }
}

void bad_user_password()
{
  HTTP::HeaderMap headers = {{"Content-Type", "text/html; charset=\"UTF-8\""},
                             {"Authorization", makeBasicAuthHash("user_b", "password_b1")}};
  HTTP::ParamMap params = {{"foo", "bar"}};
  HTTP::Request request(headers, "", "1.1", params, "/foo", HTTP::RequestMethod::GET, false);
  HTTP::Authentication auth(true);
  auth.addUser("user_a", "password_a");
  auth.addUser("user_b", "password_b");
  HTTP::Response response;
  if (auth.authenticateRequest(request, response))
  {
    TEST_FAILED("Bad password accepted");
  }
  else
  {
    TEST_PASSED();
  }
}

void no_such_user()
{
  HTTP::HeaderMap headers = {{"Content-Type", "text/html; charset=\"UTF-8\""},
                             {"Authorization", makeBasicAuthHash("user_c", "password_c")}};
  HTTP::ParamMap params = {{"foo", "bar"}};
  HTTP::Request request(headers, "", "1.1", params, "", HTTP::RequestMethod::GET, false);
  HTTP::Authentication auth(true);
  auth.addUser("user_a", "password_a");
  auth.addUser("user_b", "password_b");
  HTTP::Response response;
  if (auth.authenticateRequest(request, response))
  {
    TEST_FAILED("Incorrect user name accepted");
  }
  else
  {
    TEST_PASSED();
  }
}

void user_access_test_1()
{
  HTTP::HeaderMap headers = {{"Content-Type", "text/html; charset=\"UTF-8\""},
                             {"Authorization", makeBasicAuthHash("foo", "bar")}};
  HTTP::ParamMap params = {{"foo", "bar"}};
  HTTP::Request request1(headers, "", "1.1", params, "/foo", HTTP::RequestMethod::GET, false);
  HTTP::Request request2(headers, "", "1.1", params, "/bar", HTTP::RequestMethod::GET, false);
  AuthenticationExtTest auth;
  auth.addUser("foo", "bar", 4);
  auth.addUser("user", "password", 2);
  HTTP::Response response;

  if (auth.authenticateRequest(request1, response))
  {
    TEST_FAILED("Access incorrectly allowed for this user");
  }
  else
  {
    TEST_PASSED();
  }

  if (auth.authenticateRequest(request2, response))
  {
    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Access incorrectly denied for this user");
  }
}

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(auth_missing);
    TEST(correct_auth_provided);
    TEST(bad_user_password);
    TEST(user_access_test_1);
  }
};

}  // namespace HTTPTest

int main()
{
  std::cout << std::endl
            << "HTTP authentication tester" << std::endl
            << "========================" << std::endl;
  HTTPTest::tests t;
  return t.run();
}
