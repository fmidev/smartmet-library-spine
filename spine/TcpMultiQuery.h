#pragma once

#include <boost/asio.hpp>
#include <map>
#include <set>

namespace SmartMet
{
namespace Spine
{
/**
 *   @brief Class for performing 0 or more simple TCP/IP requests
 *
 *   Requests are performed in parallel if more than one request sare specified.
 *
 *   Only simple requests (send data to server and receive response) are supported
 *
 *   Some improvement ideas:
 *   - SSL support
 */
class TcpMultiQuery
{
 public:
  struct Response
  {
    /**
     *   @brief Response body received (may be empty or incomplete in case of
     *          an error)
     */
    const std::string body;

    /**
     *   @brief Description of operation which caused an error
     */
    const std::string error_desc;

    /**
     *   @brief Indicates error code in case of an error (not 0)
     */
    const boost::system::error_code error_code;

    Response(const std::string& body,
             const std::string& error_desc,
             boost::system::error_code error_code)

        : body(body), error_desc(error_desc), error_code(error_code)
    {
    }
  };

 public:
  TcpMultiQuery(int timeout_sec);
  virtual ~TcpMultiQuery();

  void add_query(const std::string& id,
                 const std::string& host,
                 const std::string& service,
                 const std::string& request_body);

  void execute();

  std::set<std::string> get_ids() const;

  Response operator[](const std::string& id) const;

 private:
  void report_request_complete();

 private:
  struct Query;

  boost::asio::io_service io_service;
  boost::asio::deadline_timer timeout;

  std::map<std::string, std::shared_ptr<Query> > query_map;

  /**
   *   @brief NUmber of finished requests (both failure and success)
   */
  std::size_t num_completed_requests;
};
}  // namespace Spine
}  // namespace SmartMet
