#pragma once

#ifdef MYDEBUG
#define BOOST_SPIRIT_DEBUG
#include <boost/spirit/include/phoenix.hpp>
#endif

#include <string>
#include <utility>
#include <vector>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include <boost/config/warning_disable.hpp>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_char_class.hpp>

namespace SmartMet
{
namespace Spine
{
namespace HTTP
{
using StringPair = std::pair<std::string, std::string>;
using VersionPair = std::pair<unsigned int, unsigned int>;

struct RawRequest
{
  std::string type;

  std::string resource;

  std::vector<StringPair> params;

  VersionPair version;

  std::vector<StringPair> headers;

  std::string body;
};

// The head of a request: everything up to and including the blank line that
// ends the header section. Parsed on its own by RequestHeadParser, so that the
// caller can decide the body length from the headers instead of swallowing the
// rest of the buffer - which is what makes reading one message at a time, and
// therefore persistent connections carrying several requests, possible.
struct RawRequestHead
{
  std::string type;

  std::string resource;

  std::vector<StringPair> params;

  VersionPair version;

  std::vector<StringPair> headers;
};

struct RawResponse
{
  VersionPair version;

  unsigned int code;

  std::string reason;

  std::vector<StringPair> headers;
};

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet

BOOST_FUSION_ADAPT_STRUCT(
    SmartMet::Spine::HTTP::RawRequest,
    (std::string, type)(std::string,
                        resource)(std::vector<SmartMet::Spine::HTTP::StringPair>,
                                  params)(SmartMet::Spine::HTTP::VersionPair,
                                          version)(std::vector<SmartMet::Spine::HTTP::StringPair>,
                                                   headers)(std::string, body))

BOOST_FUSION_ADAPT_STRUCT(
    SmartMet::Spine::HTTP::RawRequestHead,
    (std::string, type)(std::string,
                        resource)(std::vector<SmartMet::Spine::HTTP::StringPair>,
                                  params)(SmartMet::Spine::HTTP::VersionPair,
                                          version)(std::vector<SmartMet::Spine::HTTP::StringPair>,
                                                   headers))

BOOST_FUSION_ADAPT_STRUCT(SmartMet::Spine::HTTP::RawResponse,
                          (SmartMet::Spine::HTTP::VersionPair,
                           version)(unsigned int,
                                    code)(std::string,
                                          reason)(std::vector<SmartMet::Spine::HTTP::StringPair>,
                                                  headers))

namespace SmartMet
{
namespace Spine
{
namespace HTTP
{
namespace qi = boost::spirit::qi;
namespace spirit = boost::spirit;

template <typename Iterator>
struct RequestParser : qi::grammar<Iterator, RawRequest()>
{
  RequestParser() : RequestParser::base_type(request)
  {
    using namespace boost::spirit::qi;

    type = +upper >> qi::omit[*spirit::ascii::blank];

    resource = +(graph - '?');

    key = *qi::lit("&") >> +(char_ - char_("=& "));

    value = *(char_ - char_("& ")) >> *qi::lit("&");

    parameter_pair = key >> -qi::lit('=') >> value;

    params = *parameter_pair >> qi::omit[*spirit::ascii::blank];

    version = qi::lit("HTTP/") >> qi::uint_ >> '.' >> qi::uint_ >> "\r\n";

    headers = *(+(qi::print - ':') >> ": " >> +(qi::char_ - qi::eol) >> "\r\n");

    body = *char_;

    request = type >> resource >> -qi::lit("?") >> params >> version >> headers >> "\r\n" >> body;

#ifdef MYDEBUG
    using namespace boost::phoenix;
    on_error<fail>(request,
                   std::cout << val("Error! Expecting ") << _4  // what failed?
                             << val(" here: \"")
                             << construct<std::string>(_3, _2)  // iterators to error-pos, end
                             << val("\"") << std::endl);

    BOOST_SPIRIT_DEBUG_NODE(type);
    BOOST_SPIRIT_DEBUG_NODE(resource);
    BOOST_SPIRIT_DEBUG_NODE(key);
    BOOST_SPIRIT_DEBUG_NODE(value);
    BOOST_SPIRIT_DEBUG_NODE(parameter_pair);
    BOOST_SPIRIT_DEBUG_NODE(params);
    BOOST_SPIRIT_DEBUG_NODE(version);
    BOOST_SPIRIT_DEBUG_NODE(body);
    BOOST_SPIRIT_DEBUG_NODE(headers);
    BOOST_SPIRIT_DEBUG_NODE(request);
#endif
  }

  qi::rule<Iterator, std::string()> type;
  qi::rule<Iterator, std::string()> resource;
  qi::rule<Iterator, std::string()> key;
  qi::rule<Iterator, std::string()> value;
  qi::rule<Iterator, std::pair<std::string, std::string>()> parameter_pair;
  qi::rule<Iterator, std::vector<StringPair>()> params;
  qi::rule<Iterator, VersionPair()> version;
  qi::rule<Iterator, std::string()> body;
  qi::rule<Iterator, void()> newline;
  qi::rule<Iterator, std::string()> header;
  qi::rule<Iterator, std::vector<StringPair>()> headers;
  qi::rule<Iterator, RawRequest()> request;
};

// Grammar for the head of a request, i.e. RequestParser without the trailing
// "body = *char_" that consumes everything left in the buffer. After a
// successful parse the iterator points at the first byte of the body, which the
// caller frames using Content-Length or the chunked transfer coding.
template <typename Iterator>
struct RequestHeadParser : qi::grammar<Iterator, RawRequestHead()>
{
  RequestHeadParser() : RequestHeadParser::base_type(request)
  {
    using namespace boost::spirit::qi;

    type = +upper >> qi::omit[*spirit::ascii::blank];

    resource = +(graph - '?');

    key = *qi::lit("&") >> +(char_ - char_("=& "));

    value = *(char_ - char_("& ")) >> *qi::lit("&");

    parameter_pair = key >> -qi::lit('=') >> value;

    params = *parameter_pair >> qi::omit[*spirit::ascii::blank];

    version = qi::lit("HTTP/") >> qi::uint_ >> '.' >> qi::uint_ >> "\r\n";

    // RFC 9112 5: "field-name ':' OWS field-value OWS". RequestParser insists on
    // exactly one space after the colon and on a non-empty value; both are
    // accepted here. A continuation line (obsolete line folding, RFC 9112 5.2)
    // still fails to parse: it carries no colon, so neither this rule nor the
    // blank line that follows can match it.
    headers = *(+(qi::print - ':') >> ':' >> qi::omit[*spirit::ascii::blank] >>
                *(qi::char_ - qi::eol) >> "\r\n");

    request = type >> resource >> -qi::lit("?") >> params >> version >> headers >> "\r\n";
  }

  qi::rule<Iterator, std::string()> type;
  qi::rule<Iterator, std::string()> resource;
  qi::rule<Iterator, std::string()> key;
  qi::rule<Iterator, std::string()> value;
  qi::rule<Iterator, std::pair<std::string, std::string>()> parameter_pair;
  qi::rule<Iterator, std::vector<StringPair>()> params;
  qi::rule<Iterator, VersionPair()> version;
  qi::rule<Iterator, std::vector<StringPair>()> headers;
  qi::rule<Iterator, RawRequestHead()> request;
};

template <typename Iterator>
struct ResponseParser : qi::grammar<Iterator, RawResponse()>
{
  ResponseParser() : ResponseParser::base_type(response)
  {
    using namespace boost::spirit::qi;

    version = qi::lit("HTTP/") >> qi::uint_ >> '.' >> qi::uint_ >> +qi::space;

    code = qi::uint_ >> +qi::space;

    reason = +((qi::char_ - qi::eol)) >> +qi::eol;

    headers = *(+(qi::print - ':') >> ": " >> +(qi::char_ - qi::eol) >> "\r\n");

    response = version >> code >> reason >> headers >> "\r\n";

#ifdef MYDEBUG
    using namespace boost::phoenix;
    on_error<fail>(response,
                   std::cout << val("Error! Expecting ") << _4  // what failed?
                             << val(" here: \"")
                             << construct<std::string>(_3, _2)  // iterators to error-pos, end
                             << val("\"") << std::endl);

    BOOST_SPIRIT_DEBUG_NODE(version);
    BOOST_SPIRIT_DEBUG_NODE(code);
    BOOST_SPIRIT_DEBUG_NODE(reason);
    BOOST_SPIRIT_DEBUG_NODE(headers);
    BOOST_SPIRIT_DEBUG_NODE(response);

#endif
  }

  qi::rule<Iterator, VersionPair()> version;
  qi::rule<Iterator, unsigned int()> code;
  qi::rule<Iterator, std::string()> reason;
  qi::rule<Iterator, std::vector<StringPair>()> headers;
  qi::rule<Iterator, RawResponse()> response;
};

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet
