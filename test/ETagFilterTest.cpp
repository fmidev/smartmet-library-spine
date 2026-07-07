#include "HTTP.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <regression/tframe.h>

using SmartMet::Spine::HTTP::conditionalResponseStatus;
using SmartMet::Spine::HTTP::ETagFilter;
using SmartMet::Spine::HTTP::ParsingStatus;
using SmartMet::Spine::HTTP::Request;
using SmartMet::Spine::HTTP::Status;

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

  // Failed If-Match precondition -> 412 Precondition Failed, no body required
  if (filter.full_response_required("\"xyz\""))
    TEST_FAILED("Failed If-Match precondition should not require a full response (412, no body)");

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

  // Weak vs strong -> precondition fails -> 412, no full response required
  if (filter.full_response_required("\"abc\""))
    TEST_FAILED("Weak If-Match must not match under strong comparison (412, no body)");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void both_headers_if_match_fails()
{
  // If-Match is evaluated first; a failed precondition wins -> 412, no body
  auto req = makeRequest({"If-Match: \"abc\"", "If-None-Match: \"xyz\""});
  ETagFilter filter(*req);

  if (filter.full_response_required("\"xyz\""))
    TEST_FAILED("If-Match failure should yield 412 (no body) even if If-None-Match matches");

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

void evaluate_no_headers()
{
  auto req = makeRequest({});
  ETagFilter filter(*req);

  auto result = filter.evaluate("\"abc\"");
  if (!result.first)
    TEST_FAILED("Full response must be required when no conditional headers present");
  if (result.second != Status::ok)
    TEST_FAILED("Suggested status must be 200 OK when no conditional headers present");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void evaluate_if_none_match_match_yields_304()
{
  auto req = makeRequest({"If-None-Match: \"abc\""});
  ETagFilter filter(*req);

  auto result = filter.evaluate("\"abc\"");
  if (result.first)
    TEST_FAILED("Matching If-None-Match should not require a full response");
  if (result.second != Status::not_modified)
    TEST_FAILED("Matching If-None-Match should suggest 304 Not Modified");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void evaluate_if_none_match_no_match_yields_200()
{
  auto req = makeRequest({"If-None-Match: \"abc\""});
  ETagFilter filter(*req);

  auto result = filter.evaluate("\"xyz\"");
  if (!result.first)
    TEST_FAILED("Non-matching If-None-Match should require a full response");
  if (result.second != Status::ok)
    TEST_FAILED("Non-matching If-None-Match should suggest 200 OK");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void evaluate_if_match_pass_yields_200()
{
  auto req = makeRequest({"If-Match: \"abc\""});
  ETagFilter filter(*req);

  auto result = filter.evaluate("\"abc\"");
  if (!result.first)
    TEST_FAILED("Matching If-Match should require a full response");
  if (result.second != Status::ok)
    TEST_FAILED("Matching If-Match should suggest 200 OK");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void evaluate_if_match_fail_yields_412()
{
  auto req = makeRequest({"If-Match: \"abc\""});
  ETagFilter filter(*req);

  auto result = filter.evaluate("\"xyz\"");
  if (result.first)
    TEST_FAILED("Failed If-Match should not require a full response (412, no body)");
  if (result.second != Status::precondition_failed)
    TEST_FAILED("Failed If-Match should suggest 412 Precondition Failed");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void evaluate_if_match_fail_wins_over_if_none_match()
{
  // If-Match is evaluated first; a failed precondition wins -> 412, not 304
  auto req = makeRequest({"If-Match: \"abc\"", "If-None-Match: \"xyz\""});
  ETagFilter filter(*req);

  auto result = filter.evaluate("\"xyz\"");
  if (result.first)
    TEST_FAILED("Failed If-Match should not require a full response");
  if (result.second != Status::precondition_failed)
    TEST_FAILED("Failed If-Match should suggest 412 even if If-None-Match matches");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void conditional_status_no_headers()
{
  auto req = makeRequest({});
  auto status = conditionalResponseStatus(*req, "\"abc\"");
  if (status)
    TEST_FAILED("No conditional headers should require the full response (nullopt)");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void conditional_status_if_none_match_match()
{
  auto req = makeRequest({"If-None-Match: \"abc\""});
  auto status = conditionalResponseStatus(*req, "\"abc\"");
  if (!status)
    TEST_FAILED("Matching If-None-Match should yield a status (304)");
  if (*status != Status::not_modified)
    TEST_FAILED("Matching If-None-Match should yield 304 Not Modified");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void conditional_status_if_none_match_no_match()
{
  auto req = makeRequest({"If-None-Match: \"abc\""});
  auto status = conditionalResponseStatus(*req, "\"xyz\"");
  if (status)
    TEST_FAILED("Non-matching If-None-Match should require the full response (nullopt)");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void conditional_status_if_match_fail()
{
  auto req = makeRequest({"If-Match: \"abc\""});
  auto status = conditionalResponseStatus(*req, "\"xyz\"");
  if (!status)
    TEST_FAILED("Failed If-Match should yield a status (412)");
  if (*status != Status::precondition_failed)
    TEST_FAILED("Failed If-Match should yield 412 Precondition Failed");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void conditional_status_probe_skips_evaluation()
{
  // While the frontend probes (X-Request-ETag), the backend must not
  // short-circuit even if a precondition matches.
  auto req = makeRequest({"X-Request-ETag: true", "If-None-Match: \"abc\""});
  auto status = conditionalResponseStatus(*req, "\"abc\"");
  if (status)
    TEST_FAILED("X-Request-ETag probe should suppress conditional shortcuts (nullopt)");

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
    TEST(evaluate_no_headers);
    TEST(evaluate_if_none_match_match_yields_304);
    TEST(evaluate_if_none_match_no_match_yields_200);
    TEST(evaluate_if_match_pass_yields_200);
    TEST(evaluate_if_match_fail_yields_412);
    TEST(evaluate_if_match_fail_wins_over_if_none_match);
    TEST(conditional_status_no_headers);
    TEST(conditional_status_if_none_match_match);
    TEST(conditional_status_if_none_match_no_match);
    TEST(conditional_status_if_match_fail);
    TEST(conditional_status_probe_skips_evaluation);
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
