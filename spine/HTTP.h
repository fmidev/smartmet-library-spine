
// ----------------------------------------------------------------------
/*!
 * \brief Functions and classes to handle HTTP messages
 */
// ----------------------------------------------------------------------
#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/optional.hpp>
#include <boost/range.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

// For asio buffer types
#include <boost/asio.hpp>

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
}

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

struct CaseInsensitiveComp : std::binary_function<std::string, std::string, bool>
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
  request_entity_too_large = 413,
  internal_server_error = 500,
  not_implemented = 501,
  bad_gateway = 502,
  service_unavailable = 503,
  // Internal shutdown response code is intentionally 4 digits to avoid clashes with official (now
  // or future) codes
  shutdown = 3210
};

// ----------------------------------------------------------------------
/*!
 * \brief HTTP Request type holder class
 */
// ----------------------------------------------------------------------

enum class RequestMethod
{
  GET,
  POST
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

  MessageContent(boost::shared_ptr<std::string> theContent);

  MessageContent(boost::shared_ptr<std::vector<char> > theContent);

  MessageContent(boost::shared_array<char> theContent, std::size_t theSize);

  MessageContent(boost::shared_ptr<ContentStreamer> theContent);

  MessageContent(boost::shared_ptr<ContentStreamer> theContent, std::size_t contentSize);

  boost::asio::const_buffers_1 getBuffer();

  std::string getString();

  std::stringstream& operator<<(std::stringstream& ss);

  MessageContent& operator+(const std::string& moreContent);

  std::size_t size() const;

  bool empty() const;

  content_type getType() const;

  ContentStreamer::StreamerStatus getStreamingStatus() const;

 private:
  std::string stringContent;
  boost::shared_ptr<std::vector<char> > vectorContent;
  boost::shared_array<char> arrayContent;
  boost::shared_ptr<ContentStreamer> streamContent;
  boost::shared_ptr<std::string> stringPtrContent;
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

  boost::optional<std::string> getHeader(const std::string& headerName) const;

  // ----------------------------------------------------------------------
  /*!
   * \brief Get protocol (http/https) from header
   */
  // ----------------------------------------------------------------------

  boost::optional<std::string> getProtocol() const;

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
   * \brief Asio::Buffer representation the message headers for socket writing
   */
  // ----------------------------------------------------------------------
  virtual boost::asio::const_buffers_1 headersToBuffer() = 0;

  // ----------------------------------------------------------------------
  /*!
   * \brief Asio::Buffer representation the message content for socket writing
   * In case of streamable content, the content will be the next chunk
   */
  // ----------------------------------------------------------------------
  virtual boost::asio::const_buffers_1 contentToBuffer() = 0;

  virtual ~Message();

 protected:
  // Only called from derived classes
  Message(const HeaderMap& headerMap, const std::string& version, bool isChunked);

  // Construct empty message
  Message();

  HeaderMap itsHeaders;

  std::string itsHeaderString;

  std::string itsVersion;

  bool itsIsChunked;
};

// ----------------------------------------------------------------------
/*!
 * \brief Class describing an HTTP Request
 */
// ----------------------------------------------------------------------

class Request : public Message
{
 public:
  // ----------------------------------------------------------------------
  /*!
   * \brief Default constructor
   */
  // ----------------------------------------------------------------------

  Request();

  // ----------------------------------------------------------------------
  /*!
   * \brief Construct from arguments
   */
  // ----------------------------------------------------------------------

  Request(const HeaderMap& headerMap,
          const std::string& body,
          const std::string& version,
          const ParamMap& theParameters,
          const std::string& resource,
          const RequestMethod& method,
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
  boost::optional<std::string> getParameter(const std::string& paramName) const;

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
  virtual boost::asio::const_buffers_1 headersToBuffer();

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
  virtual boost::asio::const_buffers_1 contentToBuffer();

  virtual ~Request();

 protected:
  std::string itsContent;

  ParamMap itsParameters;

  RequestMethod itsMethod;

  std::string itsResource;

  std::string itsClientIP;

  bool itsHasParsedPostData;
};

class Response : public Message
{
 public:
  friend class SmartMet::Proxy;
  friend class SmartMet::Server::AsyncConnection;
  friend class SmartMet::Server::SyncConnection;

  // ----------------------------------------------------------------------
  /*!
   * \brief Default constructor
   */
  // ----------------------------------------------------------------------
  Response();

  // ----------------------------------------------------------------------
  /*!
   * \brief Construct from arguments
   */
  // ----------------------------------------------------------------------
  Response(const HeaderMap& headerMap,
           const std::string& body,
           const std::string& version,
           const Status status,
           const std::string& reason,
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
  void setContent(boost::shared_ptr<std::string> theContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (pointer to vector)
   */
  // ----------------------------------------------------------------------
  void setContent(boost::shared_ptr<std::vector<char> > theContent);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (shared array)
   */
  // ----------------------------------------------------------------------
  void setContent(boost::shared_array<char> theContent, std::size_t contentSize);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (stream content with known size)
   */
  // ----------------------------------------------------------------------
  void setContent(boost::shared_ptr<ContentStreamer> theContent, std::size_t contentSize);

  // ----------------------------------------------------------------------
  /*!
   * \brief Set message content (stream content with unknown size)
   * This implies the use of chunked transfer encoding
   */
  // ----------------------------------------------------------------------
  void setContent(boost::shared_ptr<ContentStreamer> theContent);

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
  virtual boost::asio::const_buffers_1 headersToBuffer();

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
  virtual boost::asio::const_buffers_1 contentToBuffer();

  virtual ~Response();

 protected:
  ContentStreamer::StreamerStatus getStreamingStatus() const;

  MessageContent itsContent;

  Status itsStatus;

  std::string itsReasonPhrase;

  bool itsHasStreamContent;

  bool isGatewayResponse;

  std::string itsOriginatingBackend;
  int itsBackendPort;
};

// ----------------------------------------------------------------------
/*!
 * \brief Parse HTTP request from std::string. Returns a ParsingStatus
 * indicating parsing status and pointer to the parsed request.
 */
// ----------------------------------------------------------------------
std::pair<ParsingStatus, std::unique_ptr<Request> > parseRequest(const std::string& message);

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

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet
