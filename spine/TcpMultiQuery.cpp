#include "TcpMultiQuery.h"
#include <boost/bind/bind.hpp>
#include <boost/chrono.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <macgyver/Exception.h>
#include <functional>

using SmartMet::Spine::TcpMultiQuery;
namespace sys = boost::system;

struct TcpMultiQuery::Query
{
  enum
  {
    buffer_size = 1024
  };

  TcpMultiQuery& client;

 public:
  const std::string host;
  const std::string service;
  const std::string request_body;
  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ip::tcp::socket socket;
  std::string result;
  std::string error_desc;
  boost::system::error_code error_code;
  bool finished;
  char current_input[buffer_size];

  Query(TcpMultiQuery& client,
        const std::string& host,
        const std::string& service,
        const std::string& request_body);

  virtual ~Query();

  void on_endpoint_resolved(boost::system::error_code,
                            boost::asio::ip::tcp::resolver::results_type result);

  void on_connected(boost::system::error_code);

  void on_request_sent(boost::system::error_code, std::size_t bytes_transfered);

  void request_input();

  void on_data_received(boost::system::error_code error, std::size_t bytes_transfered);

  void report_error(const std::string& desc, boost::system::error_code error);
};

TcpMultiQuery::TcpMultiQuery(int timeout_sec)
    : io_service(), timeout(io_service), query_map(), num_completed_requests(0)
{
  timeout.expires_from_now(boost::posix_time::seconds(timeout_sec));

  timeout.async_wait(
      [this](boost::system::error_code error_code)
      {
        if (error_code != boost::asio::error::operation_aborted)
        {
          io_service.stop();
        }
      });
}

TcpMultiQuery::~TcpMultiQuery()
{
  io_service.stop();
}

void TcpMultiQuery::add_query(const std::string& id,
                              const std::string& host,
                              const std::string& service,
                              const std::string& request_body)
{
  auto item = std::make_shared<Query>(*this, host, service, request_body);
  if (query_map.emplace(id, item).second)
  {
    using namespace std::placeholders;
    item->resolver.async_resolve(
        host, service, std::bind(&Query::on_endpoint_resolved, &*item, _1, _2));
  }
  else
  {
    Fmi::Exception ex(BCP, "Duplicate query ID " + id);
    ex.addParameter("host", host);
    ex.addParameter("service", service);
    throw ex;
  }
}

void TcpMultiQuery::execute()
{
  if (not query_map.empty())
  {
    io_service.run();

    for (const auto& query : query_map)
    {
      if (!query.second->finished)
      {
        sys::error_code err(sys::errc::timed_out, sys::generic_category());
        query.second->error_desc = "";
        query.second->error_code = err;
      }
    }
  }
}

std::set<std::string> TcpMultiQuery::get_ids() const
{
  std::set<std::string> result;
  for (const auto& item : query_map)
  {
    result.insert(item.first);
  }
  return result;
}

TcpMultiQuery::Response TcpMultiQuery::operator[](const std::string& id) const
{
  const auto iter = query_map.find(id);
  if (iter == query_map.end())
  {
    throw Fmi::Exception(BCP, id + " is not found");
  }
  else
  {
    return Response(iter->second->result, iter->second->error_desc, iter->second->error_code);
  }
}

void TcpMultiQuery::report_request_complete()
{
  if (++num_completed_requests >= query_map.size())
  {
    timeout.cancel();
  }
}

TcpMultiQuery::Query::Query(TcpMultiQuery& client,
                            const std::string& host,
                            const std::string& service,
                            const std::string& request_body)

    : client(client),
      host(host),
      service(service),
      request_body(request_body),
      resolver(client.io_service),
      socket(client.io_service),
      finished(false)
{
}

TcpMultiQuery::Query::~Query() {}

void TcpMultiQuery::Query::on_endpoint_resolved(boost::system::error_code error,
                                                boost::asio::ip::tcp::resolver::results_type result)
{
  if (error)
  {
    report_error("Failed to resolve", error);
  }
  else
  {
    using namespace std::placeholders;
    async_connect(socket, result, std::bind(&Query::on_connected, this, _1));
  }
}

void TcpMultiQuery::Query::on_connected(boost::system::error_code error)
{
  if (error)
  {
    report_error("Failed to connect", error);
  }
  else
  {
    using namespace std::placeholders;
    async_write(socket,
                boost::asio::buffer(request_body.data(), request_body.length()),
                std::bind(&Query::on_request_sent, this, _1, _2));
  }
}

void TcpMultiQuery::Query::on_request_sent(boost::system::error_code error,
                                           std::size_t bytes_transfered)
{
  (void)bytes_transfered;  // We requested to send all bytes, so no need to check
  if (error)
  {
    report_error("Failed to send request to socket", error);
  }
  else
  {
    boost::system::error_code ignored;
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
    request_input();
  }
}

void TcpMultiQuery::Query::on_data_received(boost::system::error_code error,
                                            std::size_t bytes_transfered)
{
  if (error == boost::asio::error::eof)
  {
    boost::system::error_code ignored;
    socket.close(ignored);
    finished = true;
    client.report_request_complete();
    // Input done
  }
  else if (error)
  {
    report_error("Failed to read data from socket", error);
  }
  else
  {
    std::string data(current_input, current_input + bytes_transfered);
    result.append(data);
    request_input();
  }
}

void TcpMultiQuery::Query::request_input()
{
  using namespace std::placeholders;
  socket.async_read_some(boost::asio::buffer(current_input, Query::buffer_size),
                         boost::bind(&Query::on_data_received,
                                     this,
                                     boost::asio::placeholders::error,
                                     boost::asio::placeholders::bytes_transferred));
}

void TcpMultiQuery::Query::report_error(const std::string& desc, boost::system::error_code error)
{
  this->error_desc = desc;
  this->error_code = error;
  client.report_request_complete();
}
