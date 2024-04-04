#pragma once

#include <boost/system/error_code.hpp>
#include <map>
#include <memory>
#include <set>
#include <string>

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

    Response(std::string body, std::string error_desc, boost::system::error_code error_code)

        : body(std::move(body)), error_desc(std::move(error_desc)), error_code(error_code)
    {
    }
  };

  explicit TcpMultiQuery(int timeout_sec);
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

  struct Query;
  struct Impl;

  std::unique_ptr<Impl> impl;

  std::map<std::string, std::shared_ptr<Query> > query_map;

  /**
   *   @brief NUmber of finished requests (both failure and success)
   */
  std::size_t num_completed_requests = 0;
};
}  // namespace Spine
}  // namespace SmartMet
