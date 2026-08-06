#include "HTTP.h"
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <regression/tframe.h>

using SmartMet::Spine::HTTP::baseETag;
using SmartMet::Spine::HTTP::contentCodedETag;
using SmartMet::Spine::HTTP::ParsingStatus;
using SmartMet::Spine::HTTP::Request;
using SmartMet::Spine::HTTP::selectContentEncoding;

namespace ContentEncodingTest
{
// The codings the SmartMet server can produce, in its own preference order,
// and the coding it answers a bare "*" with
const std::vector<std::string> supported{"zstd", "gzip"};
const std::string wildcard_coding = "gzip";

// Negotiate for the given Accept-Encoding field value
std::string negotiate(const std::optional<std::string>& accept_encoding)
{
  return selectContentEncoding(accept_encoding, supported, wildcard_coding);
}

void check(const std::optional<std::string>& accept_encoding, const std::string& expected)
{
  const std::string result = negotiate(accept_encoding);
  if (result != expected)
    TEST_FAILED("Accept-Encoding: " + accept_encoding.value_or("(missing)") + " should select '" +
                expected + "', not '" + result + "'");
}

// ----------------------------------------------------------------------

void no_accept_encoding()
{
  // Nothing may be encoded unsolicited
  check(std::nullopt, "");
  check(std::string(""), "");
  check(std::string("   "), "");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void preference_order()
{
  // The server prefers zstd over gzip when both are acceptable
  check(std::string("gzip, deflate, br, zstd"), "zstd");
  check(std::string("zstd"), "zstd");
  check(std::string("gzip, deflate"), "gzip");
  check(std::string("gzip"), "gzip");

  // Codings we cannot produce
  check(std::string("br"), "");
  check(std::string("deflate"), "");
  check(std::string("br, deflate"), "");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void case_and_whitespace_insensitivity()
{
  check(std::string("GZIP"), "gzip");
  check(std::string("  gzip  ,  deflate  "), "gzip");
  check(std::string("ZStd, GZip"), "zstd");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void refused_coding()
{
  // A refused coding must not be selected even though its name is present in
  // the header. This is the bug that made zstd responses reach clients which
  // had explicitly said they cannot decode them.
  check(std::string("gzip, deflate, zstd;q=0"), "gzip");
  check(std::string("zstd;q=0"), "");
  check(std::string("zstd;q=0, gzip;q=0"), "");
  check(std::string("zstd;q=0.000"), "");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void quality_values_rank_codings()
{
  // The highest quality value wins, not the server's own preference
  check(std::string("zstd;q=0.1, gzip;q=0.9"), "gzip");
  check(std::string("zstd;q=0.9, gzip;q=0.1"), "zstd");

  // Equal quality values leave the choice to the server
  check(std::string("zstd;q=0.5, gzip;q=0.5"), "zstd");

  // Spelling variants of the parameter
  check(std::string("zstd;Q=0, gzip"), "gzip");
  check(std::string("gzip;q=1.0"), "gzip");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void identity_preferred()
{
  // The client prefers the unencoded representation over what we could encode
  check(std::string("identity, gzip;q=0.5"), "");
  check(std::string("identity;q=1, zstd;q=0.5, gzip;q=0.5"), "");

  // ... but a coding that is at least as good as identity is used
  check(std::string("identity;q=0.5, gzip"), "gzip");
  check(std::string("identity;q=0, gzip"), "gzip");
  check(std::string("identity, gzip"), "gzip");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void wildcard()
{
  // "*" states no preference, so the most interoperable coding is used
  // instead of the newest one
  check(std::string("*"), "gzip");
  check(std::string("*;q=1"), "gzip");

  // An explicitly named coding beats the wildcard
  check(std::string("*, zstd"), "zstd");
  check(std::string("zstd;q=0, *"), "gzip");

  // The wildcard refuses everything it covers
  check(std::string("*;q=0"), "");
  check(std::string("*;q=0, gzip"), "gzip");

  // The wildcard also gives the quality value of the identity representation
  check(std::string("*;q=0.1, gzip;q=0.9"), "gzip");

  // With no wildcard coding to offer, "*" yields the identity representation
  if (!selectContentEncoding(std::string("*"), supported).empty())
    TEST_FAILED("'*' must yield identity when the caller offers no wildcard coding");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void malformed_values()
{
  // A quality value we fail to parse is ignored rather than taken to mean
  // that the coding is unacceptable
  check(std::string("gzip;q=abc"), "gzip");
  check(std::string("gzip;q="), "gzip");
  check(std::string("gzip;foo=bar"), "gzip");

  // Stray separators
  check(std::string(",gzip,"), "gzip");
  check(std::string(";;;"), "");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void negotiate_from_request()
{
  std::string message = "GET /resource HTTP/1.1\r\nAccept-Encoding: gzip, deflate\r\n\r\n";
  auto parsed = SmartMet::Spine::HTTP::parseRequest(message);
  if (parsed.first != ParsingStatus::COMPLETE)
    TEST_FAILED("Failed to parse the test request");

  if (selectContentEncoding(*parsed.second, supported, wildcard_coding) != "gzip")
    TEST_FAILED("Request overload should select gzip for 'gzip, deflate'");

  // A request without the header must not be encoded
  message = "GET /resource HTTP/1.1\r\n\r\n";
  parsed = SmartMet::Spine::HTTP::parseRequest(message);
  if (parsed.first != ParsingStatus::COMPLETE)
    TEST_FAILED("Failed to parse the test request");

  if (!selectContentEncoding(*parsed.second, supported, wildcard_coding).empty())
    TEST_FAILED("Request without Accept-Encoding must yield the identity representation");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void coded_etags()
{
  if (contentCodedETag("\"abc-timeseries\"", "zstd") != "\"abc-timeseries+zstd\"")
    TEST_FAILED("The coding must be appended inside the quotes of the opaque-tag");

  if (contentCodedETag("W/\"abc\"", "gzip") != "W/\"abc+gzip\"")
    TEST_FAILED("A weak entity-tag must keep its W/ prefix");

  if (contentCodedETag("abc", "gzip") != "abc+gzip")
    TEST_FAILED("An unquoted entity-tag must be suffixed as well");

  if (contentCodedETag("\"abc\"", "ZSTD") != "\"abc+zstd\"")
    TEST_FAILED("The coding name must be normalized to lower case");

  // The identity representation keeps the entity-tag of the data
  if (contentCodedETag("\"abc\"", "") != "\"abc\"")
    TEST_FAILED("An empty coding must return the entity-tag unchanged");
  if (contentCodedETag("\"abc\"", "identity") != "\"abc\"")
    TEST_FAILED("The identity coding must return the entity-tag unchanged");

  // Idempotent: the coding is replaced, not appended twice
  if (contentCodedETag(contentCodedETag("\"abc\"", "zstd"), "zstd") != "\"abc+zstd\"")
    TEST_FAILED("Encoding the same coding twice must not append it twice");
  if (contentCodedETag(contentCodedETag("\"abc\"", "zstd"), "gzip") != "\"abc+gzip\"")
    TEST_FAILED("Re-encoding must replace the previous coding");
  if (contentCodedETag(contentCodedETag("\"abc\"", "zstd"), "") != "\"abc\"")
    TEST_FAILED("Decoding to identity must strip the coding");

  if (!contentCodedETag("", "zstd").empty())
    TEST_FAILED("An empty entity-tag must stay empty");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void base_etags()
{
  if (baseETag("\"abc+zstd\"") != "\"abc\"")
    TEST_FAILED("The coding must be stripped from inside the quotes");

  if (baseETag("W/\"abc+gzip\"") != "W/\"abc\"")
    TEST_FAILED("A weak entity-tag must keep its W/ prefix");

  if (baseETag("abc+gzip") != "abc")
    TEST_FAILED("The coding must be stripped from an unquoted entity-tag");

  if (baseETag("\"abc+ZSTD\"") != "\"abc\"")
    TEST_FAILED("The coding name comparison must be case insensitive");

  // Tags that name no coding we know are opaque values and must survive
  if (baseETag("\"abc\"") != "\"abc\"")
    TEST_FAILED("An entity-tag without a coding must be returned unchanged");
  if (baseETag("\"abc+something\"") != "\"abc+something\"")
    TEST_FAILED("A '+' that does not introduce a known coding must survive");
  if (baseETag("\"a+b+gzip\"") != "\"a+b\"")
    TEST_FAILED("Only the trailing coding must be stripped");
  if (!baseETag("").empty())
    TEST_FAILED("An empty entity-tag must stay empty");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }

  void test()
  {
    TEST(no_accept_encoding);
    TEST(preference_order);
    TEST(case_and_whitespace_insensitivity);
    TEST(refused_coding);
    TEST(quality_values_rank_codings);
    TEST(identity_preferred);
    TEST(wildcard);
    TEST(malformed_values);
    TEST(negotiate_from_request);
    TEST(coded_etags);
    TEST(base_etags);
  }
};

}  // namespace ContentEncodingTest

int main()
{
  std::cout << std::endl
            << "Content encoding tester" << std::endl
            << "=======================" << std::endl;
  ContentEncodingTest::tests t;
  return t.run();
}
