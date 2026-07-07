#include "HTTP.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <regression/tframe.h>

using SmartMet::Spine::HTTP::ETagFilter;
using SmartMet::Spine::HTTP::ParsingStatus;
using SmartMet::Spine::HTTP::Request;

namespace ETagFilterTest
{
// Build a request with the given raw header lines (e.g. {"If-None-Match: \"abc\""})
std::unique_ptr<Request> makeRequest(const std::vector<std::string>& headers)
{
  std::string message = "GET /resource HTTP/1.1\r\n";
  for (const auto& h : headers)
    message += h + "\r\n";
  message += "\r\n";

  auto parsed = SmartMet::Spine::HTTP::parseRequest(message);
  if (parsed.first != ParsingStatus::COMPLETE)
    throw std::runtime_error("Failed to parse test request: " + message);
  return std::move(parsed.second);
}

// ----------------------------------------------------------------------

void no_conditional_headers()
{
  auto req = makeRequest({});
  ETagFilter filter(*req);

  if (filter.has_if_match())
    TEST_FAILED("has_if_match should be false when no If-Match header present");
  if (filter.has_if_none_match())
    TEST_FAILED("has_if_none_match should be false when no If-None-Match header present");

  // Full response is required for any ETag value
  if (!filter.full_response_required("\"abc\""))
    TEST_FAILED("Full response must be required when no conditional headers present");
  if (!filter.full_response_required("\"anything\""))
    TEST_FAILED("Full response must be required for any ETag when no conditional headers present");
  if (!filter.full_response_required(""))
    TEST_FAILED(
        "Full response must be required for empty ETag when no conditional headers present");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_none_match_matching()
{
  auto req = makeRequest({"If-None-Match: \"abc\""});
  ETagFilter filter(*req);

  if (!filter.has_if_none_match())
    TEST_FAILED("has_if_none_match should be true");

  // Matching ETag -> 304 Not Modified -> full response NOT required
  if (filter.full_response_required("\"abc\""))
    TEST_FAILED("Matching If-None-Match should not require a full response (304 expected)");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_none_match_not_matching()
{
  auto req = makeRequest({"If-None-Match: \"abc\""});
  ETagFilter filter(*req);

  // Non-matching ETag -> full response required
  if (!filter.full_response_required("\"xyz\""))
    TEST_FAILED("Non-matching If-None-Match should require a full response");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_none_match_multiple()
{
  auto req = makeRequest({"If-None-Match: \"abc\", \"def\", \"ghi\""});
  ETagFilter filter(*req);

  if (filter.full_response_required("\"def\""))
    TEST_FAILED("Matching one of several If-None-Match tags should not require a full response");
  if (!filter.full_response_required("\"zzz\""))
    TEST_FAILED("ETag not in the If-None-Match list should require a full response");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_none_match_wildcard()
{
  auto req = makeRequest({"If-None-Match: *"});
  ETagFilter filter(*req);

  // "*" matches any current representation -> 304
  if (filter.full_response_required("\"abc\""))
    TEST_FAILED("If-None-Match: * should not require a full response for an existing resource");
  if (filter.full_response_required("\"whatever\""))
    TEST_FAILED("If-None-Match: * should match any ETag value");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_none_match_weak_comparison()
{
  // If-None-Match uses weak comparison: W/"abc" matches "abc"
  auto req = makeRequest({"If-None-Match: W/\"abc\""});
  ETagFilter filter(*req);

  if (filter.full_response_required("\"abc\""))
    TEST_FAILED("Weak If-None-Match should match a strong resource ETag with same opaque value");
  if (filter.full_response_required("W/\"abc\""))
    TEST_FAILED("Weak If-None-Match should match a weak resource ETag with same opaque value");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_match_matching()
{
  auto req = makeRequest({"If-Match: \"abc\""});
  ETagFilter filter(*req);

  if (!filter.has_if_match())
    TEST_FAILED("has_if_match should be true");

  // Matching If-Match precondition -> proceed with full response
  if (!filter.full_response_required("\"abc\""))
    TEST_FAILED("Matching If-Match should require a full response");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_match_not_matching()
{
  auto req = makeRequest({"If-Match: \"abc\""});
  ETagFilter filter(*req);

  // Failed If-Match precondition -> not a 304, a full (error) response is required
  if (!filter.full_response_required("\"xyz\""))
    TEST_FAILED("Failed If-Match precondition should require a full response (not 304)");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_match_wildcard()
{
  auto req = makeRequest({"If-Match: *"});
  ETagFilter filter(*req);

  if (!filter.full_response_required("\"abc\""))
    TEST_FAILED("If-Match: * should require a full response for an existing resource");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void if_match_uses_strong_comparison()
{
  // If-Match uses strong comparison: a weak tag must never match
  auto req = makeRequest({"If-Match: W/\"abc\""});
  ETagFilter filter(*req);

  // Weak vs strong -> precondition fails -> full response required
  if (!filter.full_response_required("\"abc\""))
    TEST_FAILED("Weak If-Match must not match under strong comparison (full response required)");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void both_headers_if_match_fails()
{
  // If-Match is evaluated first; a failed precondition wins -> full response
  auto req = makeRequest({"If-Match: \"abc\"", "If-None-Match: \"xyz\""});
  ETagFilter filter(*req);

  if (!filter.full_response_required("\"xyz\""))
    TEST_FAILED("If-Match failure should require full response even if If-None-Match matches");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void both_headers_if_match_ok_if_none_match_matches()
{
  auto req = makeRequest({"If-Match: \"abc\"", "If-None-Match: \"abc\""});
  ETagFilter filter(*req);

  // If-Match passes, then If-None-Match matches -> 304
  if (filter.full_response_required("\"abc\""))
    TEST_FAILED("If-Match ok + If-None-Match match should yield 304 (no full response)");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void opaque_tag_with_comma()
{
  // A comma inside a quoted opaque-tag must not split the list
  auto req = makeRequest({"If-None-Match: \"a,b\", \"c\""});
  ETagFilter filter(*req);

  if (filter.full_response_required("\"a,b\""))
    TEST_FAILED("Opaque tag containing a comma should still match");
  if (filter.full_response_required("\"c\""))
    TEST_FAILED("Second tag in the list should still match");
  if (!filter.full_response_required("\"a\""))
    TEST_FAILED("Partial tag should not match");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }

  void test()
  {
    TEST(no_conditional_headers);
    TEST(if_none_match_matching);
    TEST(if_none_match_not_matching);
    TEST(if_none_match_multiple);
    TEST(if_none_match_wildcard);
    TEST(if_none_match_weak_comparison);
    TEST(if_match_matching);
    TEST(if_match_not_matching);
    TEST(if_match_wildcard);
    TEST(if_match_uses_strong_comparison);
    TEST(both_headers_if_match_fails);
    TEST(both_headers_if_match_ok_if_none_match_matches);
    TEST(opaque_tag_with_comma);
  }
};

}  // namespace ETagFilterTest

int main()
{
  std::cout << std::endl
            << "ETagFilter tester" << std::endl
            << "========================" << std::endl;
  ETagFilterTest::tests t;
  return t.run();
}
