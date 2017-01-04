#include "HTTP.h"
#include "HTTPParsers.h"
#include "Exception.h"
#include <macgyver/String.h>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_array.hpp>

#include <vector>
#include <list>
#include <utility>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

// Code from http://www.boost.org/doc/libs/1_52_0/tools/inspect/link_check.cpp
// Boost inspect tools parse URIs etc to validate documentation

namespace
{
// Decode percent encoded characters. Ignores failed conversions so control characters
// can be sent onwards.

std::string decode_percents(std::string const& url_path)
{
  try
  {
    std::string::size_type pos = 0, next;
    std::string result;
    result.reserve(url_path.length());

    while ((next = url_path.find('%', pos)) != std::string::npos)
    {
      result.append(url_path, pos, next - pos);
      pos = next;
      switch (url_path[pos])
      {
        case '%':
        {
          if (url_path.length() - next < 3)
          {
            result.append(url_path, pos, url_path.length() - next);
            pos = url_path.length();
            break;
          }
          char hex[3] = {url_path[next + 1], url_path[next + 2], '\0'};
          char* end_ptr;
          char res = static_cast<char>(std::strtol(hex, &end_ptr, 16));
          if (*end_ptr)
          {
            result += "%";
            pos = next + 1;
            break;
          }
          result += res;
          pos = next + 3;
          break;
        }
      }
    }

    result.append(url_path, pos, url_path.length());
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

#if 0
  inline long int hex2dec(const std::string& hexString)
  {
	return std::strtol(hexString.c_str(), NULL, 16);
  }
#endif

std::string char2hex(int dec)
{
  try
  {
    const char* hd = "0123456789ABCDEF";
    char dig[2] = {hd[(dec & 0xF0) >> 4], hd[dec & 0x0F]};

    return std::string(dig, dig + 2);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string urlEncode(const std::string& c)
{
  try
  {
    std::string escaped;
    std::size_t max = c.length();
    for (std::size_t i = 0; i < max; i++)
    {
      if ((48 <= c[i] && c[i] <= 57) ||   // 0-9
          (65 <= c[i] && c[i] <= 90) ||   // ABC...XYZ
          (97 <= c[i] && c[i] <= 122) ||  // abc...xyz
          (c[i] == '~' || c[i] == '-' || c[i] == '_' || c[i] == '.'))
      {
        escaped.append(&c[i], 1);
      }
      else
      {
        escaped.append("%");
        escaped.append(char2hex(static_cast<int>(c[i])));  // converts char 255 to string "FF"
      }
    }
    return escaped;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void parseTokens(SmartMet::Spine::HTTP::ParamMap& outputMap,
                 const std::string& inputString,
                 const std::string& delimiter,
                 bool convert_linebreaks = false)
{
  try
  {
    std::list<boost::iterator_range<std::string::const_iterator> > getTokens;
    auto get_range = boost::make_iterator_range(inputString.begin(), inputString.end());
    boost::algorithm::split(getTokens, get_range, boost::is_any_of(delimiter));
    if (convert_linebreaks)
    {
      for (auto it = getTokens.begin(); it != getTokens.end(); ++it)
      {
        auto delimiter_range = boost::algorithm::find_first(*it, "=");
        auto first = std::string(it->begin(), delimiter_range.begin());
        auto second = std::string(delimiter_range.end(), it->end());

        boost::algorithm::replace_all(second, "+", " ");      // replace plusses with spaces
        boost::algorithm::replace_all(second, "\r\n", "\n");  // Replace line breaks
        second = decode_percents(second);

        outputMap.insert(std::make_pair(first, second));
      }
    }
    else
    {
      // Same as above, but no line break conversion
      for (auto it = getTokens.begin(); it != getTokens.end(); ++it)
      {
        auto delimiter_range = boost::algorithm::find_first(*it, "=");
        auto first = std::string(it->begin(), delimiter_range.begin());
        auto second = std::string(delimiter_range.end(), it->end());

        boost::algorithm::replace_all(second, "+", " ");  // replace plusses with spaces
        second = decode_percents(second);

        outputMap.insert(std::make_pair(first, second));
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // anonymous namespace

namespace SmartMet
{
namespace Spine
{
namespace HTTP
{
namespace StatusStrings
{
const std::string ok = "OK";
const std::string created = "Created";
const std::string accepted = "Accepted";
const std::string no_content = "No Content";
const std::string multiple_choices = "Multiple Choices";
const std::string moved_permanently = "Moved Permanently";
const std::string moved_temporarily = "Moved Temporarily";
const std::string not_modified = "Not Modified";
const std::string bad_request = "Bad Request";
const std::string unauthorized = "Unauthorized";
const std::string forbidden = "Forbidden";
const std::string not_found = "Not Found";
const std::string internal_server_error = "Internal Server Error";
const std::string not_implemented = "Not Implemented";
const std::string bad_gateway = "Bad Gateway";
const std::string service_unavailable = "Service Unavailable";
const std::string length_required = "Length Required";
const std::string request_entity_too_large = "Request Entity Too Large";
const std::string request_timeout = "Request Timeout";

std::string statusCodeToString(Status theStatus)
{
  try
  {
    switch (theStatus)
    {
      case Status::ok:
        return ok;
      case Status::created:
        return created;
      case Status::accepted:
        return accepted;
      case Status::no_content:
        return no_content;
      case Status::multiple_choices:
        return multiple_choices;
      case Status::moved_permanently:
        return moved_permanently;
      case Status::moved_temporarily:
        return moved_temporarily;
      case Status::not_modified:
        return not_modified;
      case Status::bad_request:
        return bad_request;
      case Status::unauthorized:
        return unauthorized;
      case Status::forbidden:
        return forbidden;
      case Status::not_found:
        return not_found;
      case Status::internal_server_error:
        return internal_server_error;
      case Status::not_implemented:
        return not_implemented;
      case Status::bad_gateway:
        return bad_gateway;
      case Status::service_unavailable:
        return service_unavailable;
      case Status::length_required:
        return length_required;
      case Status::request_entity_too_large:
        return request_entity_too_large;
      case Status::request_timeout:
        return request_timeout;
      case Status::not_a_status:
        return internal_server_error;
#ifndef UNREACHABLE
      default:
        return internal_server_error;
#endif
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}  // namespace StatusStrings

namespace StockReplies
{
const std::string ok = "";
const std::string created =
    "<html>"
    "<head><title>Created</title></head>"
    "<body><h1>201 Created</h1></body>"
    "</html>";
const std::string accepted =
    "<html>"
    "<head><title>Accepted</title></head>"
    "<body><h1>202 Accepted</h1></body>"
    "</html>";
const std::string no_content = "";
const std::string multiple_choices =
    "<html>"
    "<head><title>Multiple Choices</title></head>"
    "<body><h1>300 Multiple Choices</h1></body>"
    "</html>";
const std::string moved_permanently =
    "<html>"
    "<head><title>Moved Permanently</title></head>"
    "<body><h1>301 Moved Permanently</h1></body>"
    "</html>";
const std::string moved_temporarily =
    "<html>"
    "<head><title>Moved Temporarily</title></head>"
    "<body><h1>302 Moved Temporarily</h1></body>"
    "</html>";
const std::string not_modified =
    "<html>"
    "<head><title>Not Modified</title></head>"
    "<body><h1>304 Not Modified</h1></body>"
    "</html>";
const std::string bad_request =
    "<html>"
    "<head><title>Bad Request</title></head>"
    "<body><h1>400 Bad Request</h1></body>"
    "</html>";
const std::string unauthorized =
    "<html>"
    "<head><title>Unauthorized</title></head>"
    "<body><h1>401 Unauthorized</h1></body>"
    "</html>";
const std::string forbidden =
    "<html>"
    "<head><title>Forbidden</title></head>"
    "<body><h1>403 Forbidden</h1></body>"
    "</html>";
const std::string not_found =
    "<html>"
    "<head><title>Not Found</title></head>"
    "<body><h1>404 Not Found</h1></body>"
    "</html>";
const std::string internal_server_error =
    "<html>"
    "<head><title>Internal Server Error</title></head>"
    "<body><h1>500 Internal Server Error</h1></body>"
    "</html>";
const std::string not_implemented =
    "<html>"
    "<head><title>Not Implemented</title></head>"
    "<body><h1>501 Not Implemented</h1></body>"
    "</html>";
const std::string bad_gateway =
    "<html>"
    "<head><title>Bad Gateway</title></head>"
    "<body><h1>502 Bad Gateway</h1></body>"
    "</html>";
const std::string service_unavailable =
    "<html>"
    "<head><title>Service Unavailable</title></head>"
    "<body><h1>503 Service Unavailable</h1></body>"
    "</html>";
const std::string length_required =
    "<html>"
    "<head><title>Service Unavailable</title></head>"
    "<body><h1>503 Service Unavailable</h1></body>"
    "</html>";
const std::string request_entity_too_large =
    "<html>"
    "<head><title>Request Entity Too Large</title></head>"
    "<body><h1>413 Request Entity Too Large</h1></body>"
    "</html>";
const std::string request_timeout =
    "<html>"
    "<head><title>Request Timeout</title></head>"
    "<body><h1>408 Request Timeout</h1></body>"
    "</html>";

std::string getStockReply(Status theStatus)
{
  try
  {
    switch (theStatus)
    {
      case Status::ok:
        return ok;
      case Status::created:
        return created;
      case Status::accepted:
        return accepted;
      case Status::no_content:
        return no_content;
      case Status::multiple_choices:
        return multiple_choices;
      case Status::moved_permanently:
        return moved_permanently;
      case Status::moved_temporarily:
        return moved_temporarily;
      case Status::not_modified:
        return not_modified;
      case Status::bad_request:
        return bad_request;
      case Status::unauthorized:
        return unauthorized;
      case Status::forbidden:
        return forbidden;
      case Status::not_found:
        return not_found;
      case Status::internal_server_error:
        return internal_server_error;
      case Status::not_implemented:
        return not_implemented;
      case Status::bad_gateway:
        return bad_gateway;
      case Status::service_unavailable:
        return service_unavailable;
      case Status::length_required:
        return length_required;
      case Status::request_entity_too_large:
        return request_entity_too_large;
      case Status::request_timeout:
        return request_timeout;
      case Status::not_a_status:
        return internal_server_error;
#ifndef UNREACHABLE
      default:
        return internal_server_error;
#endif
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}

Status stringToStatusCode(const std::string& theCode)
{
  try
  {
    Status returnStatus;

    if (theCode == "200")
    {
      returnStatus = Status::ok;
    }
    else if (theCode == "201")
    {
      returnStatus = Status::created;
    }
    else if (theCode == "202")
    {
      returnStatus = Status::accepted;
    }
    else if (theCode == "204")
    {
      returnStatus = Status::no_content;
    }
    else if (theCode == "300")
    {
      returnStatus = Status::multiple_choices;
    }
    else if (theCode == "301")
    {
      returnStatus = Status::moved_permanently;
    }
    else if (theCode == "302")
    {
      returnStatus = Status::moved_temporarily;
    }
    else if (theCode == "304")
    {
      returnStatus = Status::not_modified;
    }
    else if (theCode == "400")
    {
      returnStatus = Status::bad_request;
    }
    else if (theCode == "401")
    {
      returnStatus = Status::unauthorized;
    }
    else if (theCode == "403")
    {
      returnStatus = Status::forbidden;
    }
    else if (theCode == "404")
    {
      returnStatus = Status::not_found;
    }
    else if (theCode == "404")
    {
      returnStatus = Status::not_found;
    }
    else if (theCode == "404")
    {
      returnStatus = Status::not_found;
    }
    else if (theCode == "408")
    {
      returnStatus = Status::request_timeout;
    }
    else if (theCode == "411")
    {
      returnStatus = Status::length_required;
    }
    else if (theCode == "413")
    {
      returnStatus = Status::request_entity_too_large;
    }
    else if (theCode == "500")
    {
      returnStatus = Status::internal_server_error;
    }
    else if (theCode == "502")
    {
      returnStatus = Status::bad_gateway;
    }
    else if (theCode == "503")
    {
      returnStatus = Status::service_unavailable;
    }
    else
    {
      // Unrecognized throws
      throw SmartMet::Spine::Exception(BCP,
                                       "Attempting to set Unsupported status code: " + theCode);
    }

    return returnStatus;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

Message::Message(const HeaderMap& headerMap, const std::string& version, bool isChunked)
    : itsHeaders(headerMap), itsHeaderString(), itsVersion(version), itsIsChunked(isChunked)
{
}

Message::Message() : itsIsChunked(false)
{
  // Default version for default constructor
  itsVersion = "1.0";
}

boost::optional<std::string> Message::getHeader(const std::string& headerName) const
{
  try
  {
    auto iterator = itsHeaders.find(headerName);
    if (iterator == itsHeaders.end())
    {
      return boost::optional<std::string>();
    }
    else
    {
      return boost::optional<std::string>(iterator->second);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

HeaderMap Message::getHeaders() const
{
  return itsHeaders;
}

void Message::setHeader(const std::string& headerName, const std::string& headerValue)
{
  try
  {
    auto it = itsHeaders.find(headerName);
    if (it == itsHeaders.end())
    {
      // Not in headers, insert
      itsHeaders.insert(std::make_pair(headerName, headerValue));
    }
    else
    {
      // Already exists, overwrite
      it->second = headerValue;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Message::removeHeader(const std::string& headerName)
{
  try
  {
    itsHeaders.erase(headerName);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string Message::getVersion() const
{
  return itsVersion;
}

Message::~Message()
{
}

Request::Request(const HeaderMap& headerMap,
                 const std::string& body,
                 const std::string& version,
                 const ParamMap& theParameters,
                 const std::string& resource,
                 const RequestMethod& method,
                 bool hasParsedPostData)
    : Message(headerMap, version, false),  // Now only unchunked requests
      itsContent(body),
      itsParameters(theParameters),
      itsMethod(method),
      itsResource(resource),
      itsClientIP(),
      itsHasParsedPostData(hasParsedPostData)
{
}

Request::Request() : Message(), itsHasParsedPostData(false)
{
}

std::string Request::getContent() const
{
  return itsContent;
}

void Request::setContent(const std::string& theContent)
{
  itsContent = theContent;
}

std::size_t Request::getContentLength() const
{
  return itsContent.size();
}

std::string Request::toString() const
{
  try
  {
    std::stringstream ss;

    std::string body;

    ss << getMethodString() << " " << itsResource;

    if (!itsParameters.empty())
    {
      std::string paramValue;

      switch (itsMethod)
      {
        case RequestMethod::GET:
        {
          // In GET-requests parameters go to URL

          auto nextToLast = itsParameters.end();
          std::advance(nextToLast, -1);

          ss << "?";

          for (auto it = itsParameters.begin(); it != nextToLast; ++it)
          {
            paramValue = it->second;
            paramValue = ::urlEncode(paramValue);
            ss << it->first << "=" << paramValue << "&";
          }

          paramValue = nextToLast->second;
          paramValue = ::urlEncode(paramValue);
          ss << nextToLast->first << "=" << paramValue;
          break;
        }

        case RequestMethod::POST:
        {
          // In case POST-message, parameters are ignored and the body
          // content must be placed explicitly

          body = itsContent;

          break;
        }
      }
    }
    else
    {
      // No parameters added, this means any body content has been given explicitly
      switch (itsMethod)
      {
        case RequestMethod::GET:
          if (!itsContent.empty())
            // Body is not meaningfull in GET-Requests
            throw SmartMet::Spine::Exception(
                BCP, "HTTP::Request: Attempting to serialize GET Request with body content");
        case RequestMethod::POST:
          body = itsContent;
      }
    }

    ss << " HTTP/" << itsVersion << "\r\n";

    for (auto iter_pair = itsHeaders.begin(); iter_pair != itsHeaders.end(); ++iter_pair)
    {
      ss << iter_pair->first << ": " << iter_pair->second << "\r\n";
    }

    // Header-body delimiter
    ss << "\r\n";

    ss << body;  // Body is empty in case of GET - requests

    return ss.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

boost::asio::const_buffers_1 Request::contentToBuffer()
{
  // TODO
  throw SmartMet::Spine::Exception(BCP, "Request::contentToBuffer not implemented");
  // return boost::asio::const_buffers_1("moi",3);
}

boost::asio::const_buffers_1 Request::headersToBuffer()
{
  // TODO
  throw SmartMet::Spine::Exception(BCP, "Request::headersToBuffer not implemented");
  // return boost::asio::const_buffers_1("moi",3);
}

std::string Request::headersToString() const
{
  // TODO
  throw SmartMet::Spine::Exception(BCP, "Request::headersToBuffer not implemented");
  // return std::string();
}

std::string Request::getClientIP() const
{
  return itsClientIP;
}

void Request::setClientIP(const std::string& ip)
{
  itsClientIP = ip;
}

RequestMethod Request::getMethod() const
{
  return itsMethod;
}

std::string Request::getMethodString() const
{
  try
  {
    std::string ret;
    switch (itsMethod)
    {
      case RequestMethod::GET:
        ret = "GET";
        break;
      case RequestMethod::POST:
        ret = "POST";
        break;
    }

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string Request::getURI() const
{
  try
  {
    std::ostringstream ss;

    ss << itsResource;
    if (!itsParameters.empty())
    {
      auto nextToLast = itsParameters.end();
      std::advance(nextToLast, -1);

      ss << "?";

      std::string paramValue;
      for (auto it = itsParameters.begin(); it != nextToLast; ++it)
      {
        paramValue = it->second;
        paramValue = ::urlEncode(paramValue);
        ss << it->first << "=" << paramValue << "&";
      }

      paramValue = nextToLast->second;
      paramValue = ::urlEncode(paramValue);
      ss << nextToLast->first << "=" << paramValue;
    }

    return ss.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string Request::getQueryString() const
{
  try
  {
    std::ostringstream ss;

    if (!itsParameters.empty())
    {
      auto nextToLast = itsParameters.end();
      std::advance(nextToLast, -1);

      ss << "?";

      std::string paramValue;
      for (auto it = itsParameters.begin(); it != nextToLast; ++it)
      {
        paramValue = it->second;
        paramValue = ::urlEncode(paramValue);
        ss << it->first << "=" << paramValue << "&";
      }

      paramValue = nextToLast->second;
      paramValue = ::urlEncode(paramValue);
      ss << nextToLast->first << "=" << paramValue;
    }

    return ss.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string Request::getResource() const
{
  return itsResource;
}

void Request::setResource(const std::string& newResource)
{
  itsResource = newResource;
}

bool Request::hasParsedPostData() const
{
  return itsHasParsedPostData;
}

void Request::setMethod(const RequestMethod& method)
{
  itsMethod = method;
}

ParamMap Request::getParameterMap() const
{
  return itsParameters;
}

boost::optional<std::string> Request::getParameter(const std::string& paramName) const
{
  try
  {
    auto params = itsParameters.equal_range(paramName);
    std::size_t numParams = std::distance(params.first, params.second);
    if (numParams > 1)
    {
      throw SmartMet::Spine::Exception(
          BCP, "More than one parameter value for parameter \"" + paramName + "\"");
    }

    else if (numParams == 0)
    {
      return boost::optional<std::string>();
    }
    else
    {
      return boost::optional<std::string>(params.first->second);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Request::setParameter(const std::string& paramName, const std::string& paramValue)
{
  try
  {
    itsParameters.erase(paramName);

    itsParameters.insert(std::make_pair(paramName, paramValue));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Request::addParameter(const std::string& paramName, const std::string& paramValue)
{
  try
  {
    itsParameters.insert(std::make_pair(paramName, paramValue));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Request::removeParameter(const std::string& paramName)
{
  try
  {
    itsParameters.erase(paramName);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::vector<std::string> Request::getParameterList(const std::string& paramName) const
{
  try
  {
    auto pair = itsParameters.equal_range(paramName);

    std::vector<std::string> ret;
    ret.reserve(std::distance(pair.first, pair.second));

    for (auto it = pair.first; it != pair.second; ++it)
    {
      ret.push_back(it->second);
    }

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::size_t Request::getParameterCount() const
{
  return itsParameters.size();
}

Request::~Request()
{
}

Response::Response(const HeaderMap& headerMap,
                   const std::string& body,
                   const std::string& version,
                   const Status status,
                   const std::string& reason,
                   bool hasStream,
                   bool isChunked)
    : Message(headerMap, version, isChunked),
      itsContent(body),
      itsStatus(status),
      itsReasonPhrase(reason),
      itsHasStreamContent(hasStream),
      isGatewayResponse(false)
{
}

Response::Response()
    : Message(), itsStatus(Status::not_a_status), isGatewayResponse(false), itsBackendPort(0)

{
  // Construct default
  // Status must be explicitly given later
}

std::string Response::getContent()
{
  try
  {
    return itsContent.getString();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::size_t Response::getContentLength() const
{
  try
  {
    return itsContent.size();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::appendContent(const std::string& bodyPart)
{
  try
  {
    itsContent = itsContent + bodyPart;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setContent(const std::string& theContent)
{
  try
  {
    itsContent = MessageContent(theContent);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setContent(boost::shared_ptr<std::string> theContent)
{
  try
  {
    itsContent = MessageContent(theContent);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setContent(boost::shared_ptr<std::vector<char> > theContent)
{
  try
  {
    itsContent = MessageContent(theContent);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setContent(boost::shared_array<char> theContent, std::size_t contentSize)
{
  try
  {
    itsContent = MessageContent(theContent, contentSize);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setContent(boost::shared_ptr<ContentStreamer> theContent)
{
  try
  {
    // Unknown content length implies chunked send
    itsContent = MessageContent(theContent);
    itsVersion = "1.1";  // Chunked transfer encoding only available in 1.1
    itsIsChunked = true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setContent(boost::shared_ptr<ContentStreamer> theContent, std::size_t contentSize)
{
  try
  {
    itsContent = MessageContent(theContent, contentSize);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setStatus(Status newStatus)
{
  try
  {
    itsStatus = newStatus;
    itsReasonPhrase = StatusStrings::statusCodeToString(newStatus);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setStatus(Status newStatus, bool defaultContent)
{
  try
  {
    setStatus(newStatus);

    if (defaultContent)
    {
      itsContent = StockReplies::getStockReply(itsStatus);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setStatus(const std::string& statusString)
{
  try
  {
    itsStatus = stringToStatusCode(statusString);
    itsReasonPhrase = StatusStrings::statusCodeToString(itsStatus);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setStatus(const std::string& statusString, bool defaultContent)
{
  try
  {
    setStatus(statusString);

    if (defaultContent)
    {
      itsContent = StockReplies::getStockReply(itsStatus);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setStatus(int statusNumber)
{
  try
  {
    std::string statusString = Fmi::to_string(statusNumber);
    itsStatus = stringToStatusCode(statusString);
    itsReasonPhrase = StatusStrings::statusCodeToString(itsStatus);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Response::setStatus(int statusNumber, bool defaultContent)
{
  try
  {
    setStatus(statusNumber);

    if (defaultContent)
    {
      itsContent = StockReplies::getStockReply(itsStatus);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

Status Response::getStatus() const
{
  return itsStatus;
}

std::string Response::getStatusString() const
{
  try
  {
    return Fmi::to_string(static_cast<int>(itsStatus));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string Response::getReasonPhrase() const
{
  return itsReasonPhrase;
}

bool Response::getChunked() const
{
  return itsIsChunked;
}

std::string Response::toString()
{
  try
  {
    return this->headersToString() + itsContent.getString();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string Response::headersToString() const
{
  try
  {
    // Response status must be explicitly given!
    if (itsStatus == Status::not_a_status)
    {
      throw SmartMet::Spine::Exception(BCP, "HTTP Response status not set.");
    }

    // Use a string instead of a stringstream to avoid global locale locks!
    std::string out = "HTTP/" + itsVersion + " " + Fmi::to_string(static_cast<int>(itsStatus)) +
                      " " + itsReasonPhrase + "\r\n";
    for (const auto& name_value : itsHeaders)
    {
      out += name_value.first;
      out += ": ";
      out += name_value.second;
      out += "\r\n";
    }
    // Header-body delimiter
    out += "\r\n";
    return out;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

boost::asio::const_buffers_1 Response::headersToBuffer()
{
  try
  {
    // Buffers don't own their memory, so they must point to member variables
    itsHeaderString = this->headersToString();
    return boost::asio::buffer(itsHeaderString);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

boost::asio::const_buffers_1 Response::contentToBuffer()
{
  try
  {
    return itsContent.getBuffer();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool Response::hasStreamContent() const
{
  try
  {
    return itsContent.getType() == MessageContent::content_type::streamType;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

ContentStreamer::StreamerStatus Response::getStreamingStatus() const
{
  return itsContent.getStreamingStatus();
}

Response::~Response()
{
}

std::pair<ParsingStatus, std::unique_ptr<Request> > parseRequest(const std::string& message)
{
  try
  {
    HeaderMap headerMap;
    ParamMap theParameters;
    RequestMethod enumMethod;
    bool hasParsedPostData = false;

    RequestParser<std::string::const_iterator> parser;
    RawRequest target;

    auto startIt = message.begin();
    auto stopIt = message.end();

    bool success = qi::parse(startIt, stopIt, parser, target);

    if (success && (startIt == stopIt))  // Parse was succesfull and entire input was consumed
    {
      // Build header and param maps
      BOOST_FOREACH (const auto& pair, target.params)
      {
        std::string first = decode_percents(pair.first);
        if (!first.empty())  // Ignore any empty parameters
        {
          std::string second = pair.second;
          boost::algorithm::replace_all(second, "+", " ");  // replace plusses with spaces
          second = decode_percents(second);
          theParameters.insert(std::make_pair(first, second));
        }
      }

      // Build header and param maps
      BOOST_FOREACH (const auto& pair, target.headers)
      {
        headerMap.insert(pair);
      }

      if (target.type == "GET")
        enumMethod = RequestMethod::GET;
      else if (target.type == "POST")
        enumMethod = RequestMethod::POST;
      else
      {
        // Unknown request type
        // Message is not GET or POST, return failed status
        return std::make_pair(ParsingStatus::FAILED, std::unique_ptr<Request>());
      }

      // If content is declared, see that length matches the received length
      auto contentHeader = headerMap.find("Content-Length");
      if (contentHeader != headerMap.end())
      {
        unsigned long decLen;
        std::string declaredLength = contentHeader->second;
        try
        {
          decLen = Fmi::stoul(declaredLength);
        }
        catch (...)
        {
          // Garbled content length, return fail
          return std::make_pair(ParsingStatus::FAILED, std::unique_ptr<Request>());
        }

        if (target.body.size() < decLen)
        {
          // Message is incomplete, return unfinished status
          return std::make_pair(ParsingStatus::INCOMPLETE, std::unique_ptr<Request>());
        }
        else if (target.body.size() > decLen)
        {
          // More data than declared, return fail
          return std::make_pair(ParsingStatus::FAILED, std::unique_ptr<Request>());
        }
      }

      // Parse known entity content if applicable
      // x-www-form-urlencoded
      auto formHeader = headerMap.find("Content-Type");
      if (formHeader != headerMap.end())
      {
        if (formHeader->second.find("application/x-www-form-urlencoded") != std::string::npos)
        {
          // Content is x-www-form-urlencoded
          ::parseTokens(theParameters, target.body, "&", true);

          hasParsedPostData = true;
        }
      }

      // Make version string
      std::string os =
          Fmi::to_string(target.version.first) + "." + Fmi::to_string(target.version.second);

      // Successfully parsed message, return true
      return std::make_pair(ParsingStatus::COMPLETE,
                            std::unique_ptr<Request>(new Request(headerMap,
                                                                 target.body,
                                                                 os,
                                                                 theParameters,
                                                                 target.resource,
                                                                 enumMethod,
                                                                 hasParsedPostData)));
    }
    else
    {
      // Incomplete parse
      // See if the \r\n\r\n token has been received. It is mandatory
      auto iterRange = boost::make_iterator_range(startIt, stopIt);
      auto result = boost::algorithm::find_first(iterRange, "\r\n\r\n");

      if (!result)
      {
        // Token is still underway, wait for it
        return std::make_pair(ParsingStatus::INCOMPLETE, std::unique_ptr<Request>());
      }
      else
      {
        // Token has arrived, so the message is garbled
        return std::make_pair(ParsingStatus::FAILED, std::unique_ptr<Request>());
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// The following does not parse response body, since it can be arbitrarily large

std::tuple<ParsingStatus, std::unique_ptr<Response>, std::string::const_iterator> parseResponse(
    const std::string& message)
{
  try
  {
    HeaderMap headerMap;

    ResponseParser<std::string::const_iterator> parser;
    RawResponse target;

    auto startIt = message.begin();
    auto stopIt = message.end();

    bool success = qi::parse(startIt, stopIt, parser, target);

    if (success)  // Parse was succesfull
    {
      // Build header map
      BOOST_FOREACH (const auto& pair, target.headers)
      {
        headerMap.insert(pair);
      }

      Status responseStatus;
      try
      {
        responseStatus =
            stringToStatusCode(std::to_string(static_cast<unsigned long long>(target.code)));
      }
      catch (std::runtime_error&)
      {
        // Unrecognized status code
        // Lets not crash here, default to 502 Bad Gateway
        responseStatus = Status::bad_gateway;
      }

      // Make version string
      std::string os =
          Fmi::to_string(target.version.first) + "." + Fmi::to_string(target.version.second);

      // Successfully parsed message, return true
      return std::make_tuple(ParsingStatus::COMPLETE,
                             std::unique_ptr<Response>(new Response(
                                 headerMap, "", os, responseStatus, target.reason, false, false)),
                             startIt);
    }
    else
    {
      // Failed or incomplete parse
      // See if header-body delimiter has come through
      auto iterRange = boost::make_iterator_range(startIt, stopIt);
      auto result = boost::algorithm::find_first(iterRange, "\r\n\r\n");

      if (!result)
      {
        // Delimiter not found, headers are still on the way
        return std::make_tuple(
            ParsingStatus::INCOMPLETE, std::unique_ptr<Response>(), message.end());
      }
      else
      {
        // Delimiter is found but failed parse. Message is garbled.
        return std::make_tuple(ParsingStatus::FAILED, std::unique_ptr<Response>(), message.end());
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// Empty constructor means empty string content
MessageContent::MessageContent()
    : stringContent(),
      vectorContent(),
      arrayContent(),
      stringPtrContent(),
      contentSize(0),
      itsType(content_type::stringType)
{
}

MessageContent::MessageContent(const std::string& theContent)
    : stringContent(theContent),
      vectorContent(),
      arrayContent(),
      streamContent(),
      stringPtrContent(),
      contentSize(theContent.size()),
      itsType(content_type::stringType)
{
}

MessageContent::MessageContent(boost::shared_ptr<std::string> theContent)
    : stringContent(),
      vectorContent(),
      arrayContent(),
      streamContent(),
      stringPtrContent(theContent),
      contentSize(theContent->size()),
      itsType(content_type::stringPtrType)
{
}

MessageContent::MessageContent(boost::shared_ptr<std::vector<char> > theContent)
    : stringContent(),
      vectorContent(theContent),
      arrayContent(),
      streamContent(),
      stringPtrContent(),
      contentSize(theContent->size()),
      itsType(content_type::vectorType)
{
}

MessageContent::MessageContent(boost::shared_array<char> theContent, std::size_t theSize)
    : stringContent(),
      vectorContent(),
      arrayContent(theContent),
      streamContent(),
      stringPtrContent(),
      contentSize(theSize),
      itsType(content_type::arrayType)

{
}

MessageContent::MessageContent(boost::shared_ptr<ContentStreamer> theContent)
    : stringContent(),
      vectorContent(),
      arrayContent(),
      streamContent(theContent),
      stringPtrContent(),
      contentSize(std::numeric_limits<std::size_t>::max()),
      itsType(content_type::streamType)
{
}

MessageContent::MessageContent(boost::shared_ptr<ContentStreamer> theContent,
                               std::size_t contentSize)
    : stringContent(),
      vectorContent(),
      arrayContent(),
      streamContent(theContent),
      stringPtrContent(),
      contentSize(contentSize),
      itsType(content_type::streamType)
{
}

boost::asio::const_buffers_1 MessageContent::getBuffer()
{
  try
  {
    switch (itsType)
    {
      case content_type::stringType:
        return boost::asio::buffer(stringContent);

      case content_type::streamType:
        stringContent =
            streamContent->getChunk();  // Get new chunk from the stream. Zero length signifies EOI
        return boost::asio::buffer(stringContent);

      case content_type::vectorType:
        return boost::asio::const_buffers_1(&(*vectorContent)[0], vectorContent->size());

      case content_type::stringPtrType:
        return boost::asio::const_buffers_1(stringPtrContent->data(), stringPtrContent->size());

      case content_type::arrayType:
        return boost::asio::const_buffers_1(arrayContent.get(), contentSize);

#ifndef UNREACHABLE
      default:
        return boost::asio::buffer(stringContent);
#endif
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string MessageContent::getString()
{
  try
  {
    switch (itsType)
    {
      case content_type::stringType:
        return stringContent;

      case content_type::streamType:
        stringContent =
            streamContent->getChunk();  // Get new chunk from the stream. Zero length signifies EOI
        return stringContent;           // Return the currently available chunk

      case content_type::vectorType:
        return std::string(vectorContent->begin(), vectorContent->end());

      case content_type::arrayType:
        return std::string(arrayContent.get(), arrayContent.get() + contentSize);

      case content_type::stringPtrType:
        return *stringPtrContent;

#ifndef UNREACHABLE
      default:
        return stringContent;
#endif
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::stringstream& MessageContent::operator<<(std::stringstream& ss)
{
  try
  {
    ss << this->getString();
    return ss;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

MessageContent& MessageContent::operator+(const std::string& moreContent)
{
  try
  {
    if (itsType == content_type::stringType)
    {
      stringContent += moreContent;
      contentSize += moreContent.size();
    }
    else
    {
      throw SmartMet::Spine::Exception(BCP, "Can't add string non-string message content");
    }

    return *this;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::size_t MessageContent::size() const
{
  return contentSize;
}

bool MessageContent::empty() const
{
  return contentSize == 0;
}

MessageContent::content_type MessageContent::getType() const
{
  return itsType;
}

ContentStreamer::StreamerStatus MessageContent::getStreamingStatus() const
{
  try
  {
    if (itsType == content_type::streamType)
    {
      return streamContent->getStatus();
    }
    else
    {
      // This may be stupid, but you shouldn't ask for that which does not exist
      return ContentStreamer::StreamerStatus::OK;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

ContentStreamer::~ContentStreamer()
{
}
}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet