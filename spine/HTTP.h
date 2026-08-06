
// ----------------------------------------------------------------------
/*!
 * \brief Functions and classes to handle HTTP messages
 */
// ----------------------------------------------------------------------
#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/logic/tribool.hpp>
#include <optional>
#include <boost/range.hpp>
#include <boost/shared_array.hpp>
#include <memory>

// For asio buffer types
#include <boost/asio/buffer.hpp>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace SmartMet
{
class Proxy;

namespace Server
{
class AsyncConnection;
class SyncConnection;
}  // namespace Server

namespace Spine
{
namespace HTTP
{
// ----------------------------------------------------------------------
/*!
 * \brief Header and query parameter keys are case-insensitive. This is
 * the comparator for them. Note: Using the locale would make the
 * comparison slow due to a global lock in gcc string streams, hence
 * we perform an ASCII comparison only.
 */
// ----------------------------------------------------------------------

struct CaseInsensitiveComp
{
  char asciilower(char ch) const
  {
    char ret = ch;
    if (ch >= 'A' && ch <= 'Z')
      ret = static_cast<char>(ch + ('a' - 'A'));
    return ret;
  }

  bool operator()(const std::string& first, const std::string& second) const
  {
    std::size_t n = std::min(first.size(), second.size());
    for (std::size_t i = 0; i < n; i++)
    {
      char ch1 = asciilower(first[i]);
      char ch2 = asciilower(second[i]);
      if (ch1 != ch2)
        return (ch1 < ch2);
    }

    return (first.size() < second.size());
  }
};

using HeaderMap = std::map<std::string, std::string, CaseInsensitiveComp>;

using ParamMap = std::multimap<std::string, std::string, CaseInsensitiveComp>;

// ----------------------------------------------------------------------
/*!
 * \brief HTTP Status codes
 */
// ----------------------------------------------------------------------

enum Status
{
  not_a_status = 0,
  ok = 200,
  created = 201,
  accepted = 202,
  no_content = 204,
  multiple_choices = 300,
  moved_permanently = 301,
  moved_temporarily = 302,
  not_modified = 304,
  bad_request = 400,
  unauthorized = 401,
  forbidden = 403,
  not_found = 404,
  request_timeout = 408,
  length_required = 411,
  expectation_failed = 417,
  precondition_failed = 412,
  request_entity_too_large = 413,
  request_header_fields_too_large = 431,
  internal_server_error = 500,
  not_implemented = 501,
  bad_gateway = 502,
  service_unavailable = 503,
  // 4 digit local error codes to avoid clashes with official codes
  high_load = 1234,
  shutdown = 3210
};

constexpr const std::string_view smartmet_error_header = "X-SmartMet-Error";

// ----------------------------------------------------------------------
/*!
 * \brief HTTP Request type holder class
 */
// ----------------------------------------------------------------------

enum class RequestMethod
{
  GET,
  POST,
  OPTIONS,
  HEAD
};

// ----------------------------------------------------------------------
/*!
 * \brief HTTP Request parsing status
 */
// ----------------------------------------------------------------------

enum class ParsingStatus
{
  COMPLETE,
  INCOMPLETE,
  FAILED
};

// ----------------------------------------------------------------------
/*!
 * \brief Base class for setting streamable content to HTTP Response
 *
 * Enables plugins to stream their content. Library user must
 * implement this base class if streamable content is desirable.
 */
// ----------------------------------------------------------------------

class ContentStreamer
{
 public:
  // ----------------------------------------------------------------------
  /*!
   * \brief Enum for streamer status reporting
   */
  // ----------------------------------------------------------------------
  enum class StreamerStatus
  {
    OK,
    EXIT_OK,
    EXIT_ERROR
  };

  // ----------------------------------------------------------------------
  /*!
   * \brief Get the next chunk of data. Empty chunk signals EOF
   */
  // ----------------------------------------------------------------------
  virtual std::string getChunk() = 0;

  ContentStreamer() : itsStatus(StreamerStatus::OK) {}
  virtual ~ContentStreamer();

  void setStatus(StreamerStatus theStatus) { itsStatus = theStatus; }
  StreamerStatus getStatus() const { return itsStatus; }

 private:
  StreamerStatus itsStatus;
};

// Helper class to hold different kinds of message contents
class MessageContent
{
 public:
  enum class content_type
  {
    stringType,
    vectorType,
    arrayType,
    streamType,
    stringPtrType
  };

  // Empty constructor means empty string content
  MessageContent();

  MessageContent(const std::string& theContent);

  explicit MessageContent(const std::shared_ptr<std::string>& theContent);

  explicit MessageContent(const std::shared_ptr<std::vector<char>>& theContent);

  MessageContent(boost::shared_array<char> theContent, std::size_t theSize);

  explicit MessageContent(std::shared_ptr<ContentStreamer> theContent);

  MessageContent(std::shared_ptr<ContentStreamer> theContent, std::size_t contentSize);

  boost::asio::const_buffer getBuffer();

  std::string getString();

  std::stringstream& operator<<(std::stringstream& ss);

  MessageContent& operator+(const std::string& moreContent);

  std::size_t size() const;

  bool empty() const;

  content_type getType() const;

  ContentStreamer::StreamerStatus getStreamingStatus() const;

 private:
  std::string stringContent;
  std::shared_ptr<std::vector<char>> vectorContent;
  boost::shared_array<char> arrayContent;
  std::shared_ptr<ContentStreamer> streamContent;
  std::shared_ptr<std::string> stringPtrContent;
  std::size_t contentSize;

  content_type itsType;
};

// ----------------------------------------------------------------------
/*!
 * \brief Base class for HTTP messages
 *
 * These are never directly built, they simply wrap common functions
 * into a base class.
 */
// ----------------------------------------------------------------------

class Message
{
 public:
  // ----------------------------------------------------------------------
  /*!
   * \brief Get HTTP header value
   */
  // ----------------------------------------------------------------------

  std::optional<std::string> getHeader(const std::string& headerName) const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get protocol (http/https) from header
   */
  // ----------------------------------------------------------------------

  std::optional<std::string> getProtocol() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get header map for debugging
   */
  // ----------------------------------------------------------------------
  HeaderMap getHeaders() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Set HTTP header (overwrites if necesssary)
   */
  // ----------------------------------------------------------------------
  void setHeader(const std::string& headerName, const std::string& headerValue);

  // ----------------------------------------------------------------------
  /*!
   * \brief Remove HTTP header
   */
  // ----------------------------------------------------------------------
  void removeHeader(const std::string& headerName);

  // ----------------------------------------------------------------------
  /*!
   * \brief Get message HTTP version
   */
  // ----------------------------------------------------------------------
  std::string getVersion() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message HTTP version, e.g. "1.1"
   *
   * A server must answer in the version the client spoke, since the two
   * disagree on whether a connection is persistent by default.
   */
  // ----------------------------------------------------------------------
  void setVersion(const std::string& version);

  // ----------------------------------------------------------------------
  /*!
   * \brief Asio::Buffer representation the message headers for socket writing
   */
  // ----------------------------------------------------------------------
  virtual boost::asio::const_buffer headersToBuffer() = 0;

  // ----------------------------------------------------------------------
  /*!
   * \brief Asio::Buffer representation the message content for socket writing
   * In case of streamable content, the content will be the next chunk
   */
  // ----------------------------------------------------------------------
  virtual boost::asio::const_buffer contentToBuffer() = 0;

  virtual ~Message();

 protected:
  // Only called from derived classes
  Message(HeaderMap headerMap, std::string version, bool isChunked);

  // Construct empty message
  Message();

  HeaderMap itsHeaders;

  std::string itsHeaderString;

  std::string itsVersion;

  bool itsIsChunked = false;
};

// ----------------------------------------------------------------------
/*!
 * \brief Class describing an HTTP Request
 */
// ----------------------------------------------------------------------

class Request : public Message
{
 public:
  Request() = default;

  // ----------------------------------------------------------------------
  /*!
   * \brief Construct from arguments
   */
  // ----------------------------------------------------------------------

  Request(HeaderMap headerMap,
          std::string body,
          std::string version,
          ParamMap theParameters,
          std::string resource,
          RequestMethod method,
          bool hasParsedPostData);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (string)
   */
  // ----------------------------------------------------------------------
  void setContent(const std::string& theContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Get content length
   */
  // ----------------------------------------------------------------------
  std::size_t getContentLength() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Set incoming client ip
   */
  // ----------------------------------------------------------------------
  void setClientIP(const std::string& ip);

  // ----------------------------------------------------------------------
  /*!
   * \brief Get incoming client ip
   */
  // ----------------------------------------------------------------------
  std::string getClientIP() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get request content
   */
  // ----------------------------------------------------------------------
  std::string getContent() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get a single GET or POST parameter value with parameter name
   * Throws runtime_error if more than one value is present
   */
  // ----------------------------------------------------------------------
  std::optional<std::string> getParameter(const std::string& paramName) const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get GET or POST parameter value list with parameter name
   * Return vector is empty if there are no matches
   */
  // ----------------------------------------------------------------------
  std::vector<std::string> getParameterList(const std::string& paramName) const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Set GET parameter value
   * This overwrites existing value(s)
   */
  // ----------------------------------------------------------------------
  void setParameter(const std::string& paramName, const std::string& paramValue);

  // ----------------------------------------------------------------------
  /*!
   * \brief Add GET parameter value
   */
  // ----------------------------------------------------------------------
  void addParameter(const std::string& paramName, const std::string& paramValue);

  // ----------------------------------------------------------------------
  /*!
   * \brief Remove GET parameter value
   */
  // ----------------------------------------------------------------------
  void removeParameter(const std::string& paramName);

  // ----------------------------------------------------------------------
  /*!
   * \brief Get number of parsed parameters
   */
  // ----------------------------------------------------------------------
  std::size_t getParameterCount() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get request method
   */
  // ----------------------------------------------------------------------
  RequestMethod getMethod() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get request method as string
   */
  // ----------------------------------------------------------------------
  std::string getMethodString() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get request URI (resource + query parameters)
   */
  // ----------------------------------------------------------------------
  std::string getURI() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get request query string (query parameters, NOT NECESSARILY
   * IN RECEIVED ORDER)
   */
  // ----------------------------------------------------------------------
  std::string getQueryString() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Set request method
   */
  // ----------------------------------------------------------------------
  void setMethod(const RequestMethod& method);

  // ----------------------------------------------------------------------
  /*!
   * \brief Get request resource
   */
  // ----------------------------------------------------------------------
  std::string getResource() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Set request resource
   */
  // ----------------------------------------------------------------------
  void setResource(const std::string& newResource);

  // ----------------------------------------------------------------------
  /*!
   * \brief See if request contains parsed POST data. The following POST
   * content types are automatically parsed:
   *
   * -application/x-www-form-urlencoded
   */
  // ----------------------------------------------------------------------
  bool hasParsedPostData() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get parsed parameter map
   */
  // ----------------------------------------------------------------------
  ParamMap getParameterMap() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief String representation
   */
  // ----------------------------------------------------------------------
  std::string toString() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Asio::Buffer representation of the headers for socket writing
   */
  // ----------------------------------------------------------------------
  boost::asio::const_buffer headersToBuffer() override;

  // ----------------------------------------------------------------------
  /*!
   * \brief String representation of the headers
   */
  // ----------------------------------------------------------------------
  std::string headersToString() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Asio::Buffer representation the content for socket writing
   */
  // ----------------------------------------------------------------------
  boost::asio::const_buffer contentToBuffer() override;

  ~Request() override;

 protected:
  std::string itsContent;

  ParamMap itsParameters;

  RequestMethod itsMethod;

  std::string itsResource;

  std::string itsClientIP;

  bool itsHasParsedPostData = false;
};

class Response : public Message
{
 public:
  friend class SmartMet::Proxy;
  friend class SmartMet::Server::AsyncConnection;
  friend class SmartMet::Server::SyncConnection;

  Response() = default;

  // ----------------------------------------------------------------------
  /*!
   * \brief Construct from arguments
   */
  // ----------------------------------------------------------------------
  Response(HeaderMap headerMap,
           std::string body,
           std::string version,
           Status status,
           std::string reason,
           bool hasStream,
           bool isChunked);

  // ----------------------------------------------------------------------
  /*!
   * \brief Get response content
   * In case of streamable content, the content will be the next chunk
   */
  // ----------------------------------------------------------------------
  std::string getContent();

  // ----------------------------------------------------------------------
  /*!
   * \brief Get decoded response content based on Content-Encoding header
   * Supports: gzip, compress, deflate, zstd, xz, and lzma encodings
   * Returns the original content if no Content-Encoding or unsupported encoding
   */
  // ----------------------------------------------------------------------
  std::string getDecodedContent();

  // ----------------------------------------------------------------------
  /*!
   * \brief Get content length
   * This returns numeric_limits<size_t>::max() if message type is stream
   * with unknown size
   */
  // ----------------------------------------------------------------------
  std::size_t getContentLength() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Append content (OBS! Can only append string to string content)
   */
  // ----------------------------------------------------------------------
  void appendContent(const std::string& bodyPart);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (string)
   */
  // ----------------------------------------------------------------------
  void setContent(const std::string& theContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (ptr to string)
   */
  // ----------------------------------------------------------------------
  void setContent(const std::shared_ptr<std::string>& theContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (pointer to vector)
   */
  // ----------------------------------------------------------------------
  void setContent(const std::shared_ptr<std::vector<char>>& theContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (shared array)
   */
  // ----------------------------------------------------------------------
  void setContent(const boost::shared_array<char>& theContent, std::size_t contentSize);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (stream content with known size)
   */
  // ----------------------------------------------------------------------
  void setContent(const std::shared_ptr<ContentStreamer>& theContent, std::size_t contentSize);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (stream content with unknown size)
   * This implies the use of chunked transfer encoding
   */
  // ----------------------------------------------------------------------
  void setContent(const std::shared_ptr<ContentStreamer>& theContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set response status
   */
  // ----------------------------------------------------------------------
  void setStatus(Status newStatus);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set response status with stock content
   */
  // ----------------------------------------------------------------------
  void setStatus(Status newStatus, bool defaultContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set response status, string version
   */
  // ----------------------------------------------------------------------
  void setStatus(const std::string& statusString);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set response status, string version
   */
  // ----------------------------------------------------------------------
  void setStatus(const std::string& statusString, bool defaultContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set response status, int  version
   */
  // ----------------------------------------------------------------------
  void setStatus(int statusNumber);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set response status with default content, int  version
   */
  // ----------------------------------------------------------------------
  void setStatus(int statusNumber, bool defaultContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Get response status
   */
  // ----------------------------------------------------------------------
  Status getStatus() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief See if Response has streamable content
   */
  // ----------------------------------------------------------------------
  bool hasStreamContent() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Deferred access-log finalizer for streamed responses.
   *
   * The body size and the true wall-clock duration of a streamed response
   * are not known when the handler returns: the data is produced and sent
   * chunk by chunk later, by the server connection layer. HandlerView sets
   * this handler for such responses; the connection invokes it exactly once,
   * once the final chunk has been sent, passing the actual number of body
   * bytes streamed. The handler then writes the access-log entry with the
   * real size and total duration. No-op for non-streamed responses.
   */
  // ----------------------------------------------------------------------
  using StreamCompletionHandler = std::function<void(const Response&, std::size_t bytesSent)>;
  void setStreamCompletionHandler(StreamCompletionHandler handler);
  bool hasStreamCompletionHandler() const;
  void runStreamCompletionHandler(std::size_t bytesSent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Get response status as as string
   */
  // ----------------------------------------------------------------------
  std::string getStatusString() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get response reason phrase
   */
  // ----------------------------------------------------------------------
  std::string getReasonPhrase() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get if this Response is to be sent using chunked encoding
   */
  // ----------------------------------------------------------------------
  bool getChunked() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get response string representation
   * Throws runtime_error if response status is not set
   */
  // ----------------------------------------------------------------------
  std::string toString();

  // ----------------------------------------------------------------------
  /*!
   * \brief Asio::Buffer representation the response headers for socket writing
   */
  // ----------------------------------------------------------------------
  boost::asio::const_buffer headersToBuffer() override;

  // ----------------------------------------------------------------------
  /*!
   * \brief String representation of the headers
   */
  // ----------------------------------------------------------------------
  std::string headersToString() const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Asio::Buffer representation the response content for socket writing
   */
  // ----------------------------------------------------------------------
  boost::asio::const_buffer contentToBuffer() override;

  // ----------------------------------------------------------------------
  /*!
   * \brief Default response to OPTIONS method
   */
  // ----------------------------------------------------------------------
  static Response stockOptionsResponse(const std::vector<std::string>& methods = {"OPTIONS",
                                                                                  "GET"});

  ~Response() override;

 protected:
  ContentStreamer::StreamerStatus getStreamingStatus() const;

  MessageContent itsContent;

  Status itsStatus = Status::not_a_status;

  std::string itsReasonPhrase;

  bool itsHasStreamContent = false;

  bool isGatewayResponse = false;

  std::string itsOriginatingBackend;
  int itsBackendPort = 0;

  StreamCompletionHandler itsStreamCompletionHandler;
};

// ----------------------------------------------------------------------
/*!
 * \brief Evaluate the conditional request headers If-Match and If-None-Match
 *
 * Given the ETag of the resource that is about to be served, this class
 * decides whether the full response body has to be returned or whether a
 * "304 Not Modified" response is sufficient.
 *
 * The required information is extracted from the HTTP::Request object at
 * construction time.
 *
 * See RFC 7232 for the exact semantics:
 *  - If-Match uses strong comparison and, when it fails, the precondition
 *    has failed (a full response is required, not a 304).
 *  - If-None-Match uses weak comparison and, when it matches, the client's
 *    cached representation is still current, so "304 Not Modified" applies.
 *  - A value of "*" matches any current representation.
 *
 * When neither header is present, a full response is always required.
 */
// ----------------------------------------------------------------------

class ETagFilter
{
 public:
  // --------------------------------------------------------------------
  /*!
   * \brief Extract If-Match and If-None-Match information from the request
   */
  // --------------------------------------------------------------------
  explicit ETagFilter(const Request& request);

  // --------------------------------------------------------------------
  /*!
   * \brief Evaluate the conditional preconditions for the given ETag
   *
   * Returns a pair {full_response_required, suggested_status} describing how
   * the caller should respond to a GET/HEAD request for a resource with the
   * given ETag (RFC 7232):
   *
   *  - {true,  Status::ok}                  Preconditions pass (or none were
   *                                         present). The caller must build
   *                                         its normal full response; the
   *                                         suggested status is only a hint
   *                                         and the caller keeps its own
   *                                         status code.
   *  - {false, Status::not_modified}        If-None-Match matched. The caller
   *                                         should send a bodyless "304 Not
   *                                         Modified".
   *  - {false, Status::precondition_failed} If-Match failed. The caller should
   *                                         send a bodyless "412 Precondition
   *                                         Failed".
   *
   * If-Match is evaluated before If-None-Match, so a failed If-Match wins
   * over a matching If-None-Match.
   */
  // --------------------------------------------------------------------
  std::pair<bool, Status> evaluate(const std::string& etag) const;

  // --------------------------------------------------------------------
  /*!
   * \brief Check whether a full response is required for the given ETag
   *
   * Convenience wrapper around evaluate(): returns true if the full response
   * (body) must be returned for the given ETag, and false if a bodyless
   * "304 Not Modified" or "412 Precondition Failed" response is sufficient.
   *
   * When neither If-Match nor If-None-Match was present in the request,
   * this always returns true for any ETag value.
   */
  // --------------------------------------------------------------------
  bool full_response_required(const std::string& etag) const;

  // --------------------------------------------------------------------
  /*!
   * \brief True if the request contained an If-Match header
   */
  // --------------------------------------------------------------------
  bool has_if_match() const { return itsHasIfMatch; }

  // --------------------------------------------------------------------
  /*!
   * \brief True if the request contained an If-None-Match header
   */
  // --------------------------------------------------------------------
  bool has_if_none_match() const { return itsHasIfNoneMatch; }

 private:
  struct EntityTag
  {
    bool weak = false;
    std::string opaque;
  };

  bool itsHasIfMatch = false;
  bool itsHasIfNoneMatch = false;
  bool itsIfMatchAny = false;      // If-Match: *
  bool itsIfNoneMatchAny = false;  // If-None-Match: *
  std::vector<EntityTag> itsIfMatch;
  std::vector<EntityTag> itsIfNoneMatch;
};

// ----------------------------------------------------------------------
/*!
 * \brief Header the SmartMet frontend sets when probing a backend for just a
 *        resource's ETag (no body).
 *
 * While this header is present a backend must not short-circuit to 304/412:
 * the frontend performs the conditional (If-Match / If-None-Match) evaluation
 * itself once it has the ETag.
 */
// ----------------------------------------------------------------------

constexpr std::string_view request_etag_header = "X-Request-ETag";

// ----------------------------------------------------------------------
/*!
 * \brief Backend-side conditional-request handling (RFC 7232)
 *
 * Returns the bodyless status to send for a resource whose current entity-tag
 * is \a etag:
 *   - Status::not_modified        when If-None-Match matches
 *   - Status::precondition_failed when If-Match fails
 * or std::nullopt when the full response body must be produced (no matching
 * precondition, or no conditional headers).
 *
 * Returns std::nullopt while the frontend is probing (request_etag_header
 * present), leaving the conditional decision to the frontend.
 *
 * This is the backend convenience layer over ETagFilter::evaluate(); callers
 * that need finer control (e.g. the frontend's own If-Modified-Since handling)
 * use ETagFilter directly.
 */
// ----------------------------------------------------------------------

std::optional<Status> conditionalResponseStatus(const Request& request, const std::string& etag);

// ----------------------------------------------------------------------
/*!
 * \brief Negotiate the content coding to use for a response (RFC 9110 12.5.3)
 *
 * \a supportedCodings lists the codings the caller is able to produce, best
 * first, so that it decides the tie-break order (for example {"zstd","gzip"}).
 * Returns the winning coding, or an empty string for the identity (not
 * encoded) representation.
 *
 * Quality values are honoured, which a plain substring search for the coding
 * name is not able to do: "gzip, deflate, zstd;q=0" explicitly *refuses* zstd
 * and must be answered with gzip. A coding is acceptable only when its
 * quality value is greater than zero, where the value is the one given for
 * the coding itself, or the one given for "*", or zero when neither is
 * present.
 *
 * The identity representation is acceptable by default, but being acceptable
 * is not a preference: a client sending "gzip;q=0.9" wants gzip rather than an
 * unencoded response. It is therefore used only when no supported coding is
 * acceptable, or when it was given a quality value of its own (by name or
 * through "*") that is higher than that of every acceptable coding.
 *
 * \a wildcardCoding is the coding to use when a supported coding is
 * acceptable through "*" alone, i.e. when the client named no coding we can
 * produce but said it accepts anything. "*" expresses no preference at all,
 * so this is deliberately a separate choice from the head of
 * \a supportedCodings: it lets the caller answer such requests with its most
 * widely interoperable coding rather than its newest one. An empty value (the
 * default) answers them with the identity representation.
 *
 * A missing Accept-Encoding header, or one whose value is empty, yields the
 * identity representation: responses are never encoded unsolicited.
 */
// ----------------------------------------------------------------------

std::string selectContentEncoding(const std::optional<std::string>& acceptEncoding,
                                  const std::vector<std::string>& supportedCodings,
                                  const std::string& wildcardCoding = "");

std::string selectContentEncoding(const Request& request,
                                  const std::vector<std::string>& supportedCodings,
                                  const std::string& wildcardCoding = "");

// ----------------------------------------------------------------------
/*!
 * \brief The content codings a SmartMet server encodes responses with
 *
 * In preference order: zstd before gzip, since at their configured levels zstd
 * is both faster and compresses better.
 *
 * The server encodes the responses, but the frontend plugin caches them one
 * variant at a time and has to negotiate the same coding to find the variant
 * the backend produced, so both use this list. Two independent lists drifting
 * apart is what made zstd responses end up in the frontend's cache of
 * unencoded responses.
 */
// ----------------------------------------------------------------------

const std::vector<std::string>& supportedContentEncodings();

// ----------------------------------------------------------------------
/*!
 * \brief The content coding to answer "Accept-Encoding: *" with
 *
 * gzip: "*" says that any coding is acceptable, which is not a reason to pick
 * the newest one over the one every client can decode.
 */
// ----------------------------------------------------------------------

const std::string& wildcardContentEncoding();

// ----------------------------------------------------------------------
/*!
 * \brief Entity-tag of the given content coded variant of a representation
 *
 * RFC 9110 4.3.4 requires distinct entity-tags for representations that
 * differ in their content coding: an entity-tag identifies a representation,
 * and the gzip and zstd encodings of the same data are different
 * representations. Plugins hash the data they produce and therefore generate
 * one coding independent entity-tag, so the coding is appended to it when the
 * response body is encoded: "abc-timeseries" becomes "abc-timeseries+zstd".
 *
 * An empty coding (or "identity") returns the entity-tag of the identity
 * representation, and an entity-tag that already names a coding has that
 * coding replaced, so the mapping is idempotent. Weak tags keep their "W/"
 * prefix.
 */
// ----------------------------------------------------------------------

std::string contentCodedETag(const std::string& etag, const std::string& coding);

// ----------------------------------------------------------------------
/*!
 * \brief Entity-tag of the identity representation
 *
 * Strips the content coding appended by contentCodedETag(), returning the
 * coding independent entity-tag the producer of the data generated. Tags that
 * do not name a known content coding are returned unchanged.
 *
 * This is what makes an entity-tag usable as a cache key that is shared by
 * all the encodings of one resource, and what lets a conditional request
 * carrying the entity-tag of an encoded variant be compared against the
 * entity-tag of the data itself.
 */
// ----------------------------------------------------------------------

std::string baseETag(const std::string& etag);

// ----------------------------------------------------------------------
/*!
 * \brief urlencode a string
 */
// ----------------------------------------------------------------------

std::string urlencode(const std::string& url);

// ----------------------------------------------------------------------
/*!
 * \brief urldecode a string
 */
// ----------------------------------------------------------------------

std::string urldecode(const std::string& url);

// ----------------------------------------------------------------------
/*!
 * \brief Parse HTTP request from std::string. Returns a ParsingStatus
 * indicating parsing status and pointer to the parsed request.
 */
// ----------------------------------------------------------------------
std::pair<ParsingStatus, std::unique_ptr<Request>> parseRequest(const std::string& message);

// ----------------------------------------------------------------------
/*!
 * \brief Result of parsing a single request out of a connection buffer
 */
// ----------------------------------------------------------------------

struct RequestParseResult
{
  ParsingStatus status = ParsingStatus::INCOMPLETE;

  /// Set only when status is COMPLETE
  std::unique_ptr<Request> request;

  /// Number of bytes of the buffer the message occupied. Meaningful only when
  /// status is COMPLETE; whatever follows belongs to the next request.
  std::size_t consumed = 0;
};

// ----------------------------------------------------------------------
/*!
 * \brief Parse exactly one HTTP request from the start of a buffer
 *
 * parseRequest() treats everything after the header section as the body, so a
 * second request arriving in the same TCP segment is silently swallowed into
 * the first one's body. That makes it unusable on a connection that carries
 * more than one request. This reads a single message instead: the body length
 * comes from Content-Length or from the chunked transfer coding, and `consumed`
 * says how much of the buffer the message took, so the caller can keep the rest
 * for the next request.
 *
 * Chunked request bodies are decoded, so the returned Request carries the
 * assembled body and the caller never sees chunk framing.
 *
 * Framing combinations that exist to smuggle a second request past a proxy are
 * rejected outright rather than resolved by precedence rules (RFC 9112 6.3):
 * Content-Length together with Transfer-Encoding, repeated Content-Length or
 * Transfer-Encoding fields whose values differ, a Content-Length that is not a
 * plain decimal number, and any transfer coding other than chunked (which
 * cannot be decoded here).
 *
 * \param buffer Bytes received so far, starting at a message boundary
 * \return status, the request when COMPLETE, and the bytes consumed
 */
// ----------------------------------------------------------------------

RequestParseResult parseOneRequest(const std::string& buffer);

// ----------------------------------------------------------------------
/*!
 * \brief Parse HTTP response from std::string. Returns a ParsingStatus and pointer to the parsed
 * response.
 * Does not parse response body, since it can be arbitrarily large. Response body must be handled
 * separately
 * after this parsing.
 */
// ----------------------------------------------------------------------
std::tuple<ParsingStatus, std::unique_ptr<Response>, std::string::const_iterator> parseResponse(
    const std::string& message);

// ----------------------------------------------------------------------
/*!
 * Parse HTTP response including body. Handles binary content with null bytes.
 * The body is stored in the Response object as a vector of chars.
 */
// ----------------------------------------------------------------------
std::pair<ParsingStatus, std::unique_ptr<Response>> parseResponseFull(
    const std::string& message);

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet
