// ======================================================================
/*!
 * \file
 * \brief Regression tests for class TcpMultiQuery
 */
// ======================================================================

#include "TcpMultiQuery.h"
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <regression/tframe.h>
#include <memory>

namespace a = boost::asio;
using boost::asio::ip::tcp;

namespace AsyncEchoServer
{
//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Adaptation for this test by Andris Pavenis <andris.pavenis@fmi.fi>

//#include <cstdlib>
//#include <iostream>
//#include <boost/bind.hpp>
//#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class server
{
  class session;

  boost::asio::io_context& io_context_;
  tcp::acceptor acceptor_;
  int sleep_ms_before_read;
  std::set<session*> sessions;

 public:
  server(boost::asio::io_context& io_context, int sleep_ms_before_read)

      : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), 0)),
        sleep_ms_before_read(sleep_ms_before_read)
  {
    start_accept();
  }

  virtual ~server()
  {
    if (not sessions.empty())
    {
      // Clean leaked sessions to avoid analyzing address sanitizer or valgrind error messages
      // Sessions leaks if io_context is stoped before sessions are finished.
      for (const auto* s : sessions)
      {
        delete s;
      }
    }
  }

  int port() const { return acceptor_.local_endpoint().port(); }

 private:
  class session
  {
   public:
    session(server& owner, boost::asio::io_context& io_context, int sleep_ms)
        : socket_(io_context), timer(io_context), sleep_ms(sleep_ms), owner(owner)
    {
    }

    tcp::socket& socket() { return socket_; }

    void start()
    {
      if (sleep_ms > 0)
      {
        timer.expires_from_now(std::chrono::milliseconds(sleep_ms));
        timer.async_wait(
            [this](boost::system::error_code e)
            {
              if (e != boost::asio::error::operation_aborted)
              {
                real_start();
              }
            });
      }
      else
      {
        real_start();
      }
    }

    void real_start()
    {
      socket_.async_read_some(boost::asio::buffer(data_, max_length),
                              boost::bind(&session::handle_read,
                                          this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    }

   private:
    void handle_read(const boost::system::error_code& error, size_t bytes_transferred)
    {
      if (!error)
      {
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(data_, bytes_transferred),
            boost::bind(&session::handle_write, this, boost::asio::placeholders::error));
      }
      else
      {
        owner.unregister(this);
        delete this;
      }
    }

    void handle_write(const boost::system::error_code& error)
    {
      if (!error)
      {
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                boost::bind(&session::handle_read,
                                            this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
      }
      else
      {
        owner.unregister(this);
        delete this;
      }
    }

    tcp::socket socket_;
    boost::asio::basic_waitable_timer<std::chrono::system_clock> timer;
    enum
    {
      max_length = 1024
    };
    char data_[max_length];
    int sleep_ms;
    server& owner;
  };

 private:
  void start_accept()
  {
    session* new_session = new session(*this, io_context_, sleep_ms_before_read);
    sessions.insert(new_session);
    acceptor_.async_accept(
        new_session->socket(),
        boost::bind(&server::handle_accept, this, new_session, boost::asio::placeholders::error));
  }

  void handle_accept(session* new_session, const boost::system::error_code& error)
  {
    if (!error)
    {
      new_session->start();
    }
    else
    {
      unregister(new_session);
      delete new_session;
    }

    start_accept();
  }

  void unregister(session* session) { sessions.erase(session); }
};

};  // namespace AsyncEchoServer

namespace TcpMultiQueryTest
{
int server_port_1 = 0;
int server_port_2 = 0;

void empty()
{
  SmartMet::Spine::TcpMultiQuery test(1);
  test.execute();
  if (not test.get_ids().empty())
  {
    TEST_FAILED("No request IDs provided");
  }
  TEST_PASSED();
}

int single_request()
{
  SmartMet::Spine::TcpMultiQuery test(1);
  test.add_query("foo", "127.0.0.1", std::to_string(server_port_1), "foo");
  test.execute();
  const auto ids = test.get_ids();
  if (ids.size() != 1)
  {
    TEST_FAILED("Excactly 1 ID expected");
  }
  if (*ids.begin() != "foo")
  {
    TEST_FAILED("ID not as excpected");
  }

  const auto result = test["foo"];
  if (result.error_code)
  {
    TEST_FAILED("Query was expected to succeed");
  }

  if (result.error_desc != "")
  {
    TEST_FAILED("Error description expected to be empty - no error");
  }

  if (result.body != "foo")
  {
    TEST_FAILED("Response body is expected to be 'foo', but got '" + result.body + "'");
  }

  TEST_PASSED();
}

int several_requests()
{
  SmartMet::Spine::TcpMultiQuery test(1);
  std::map<std::string, std::string> test_data;
  int size = 1;
  for (int i = 0; i < 16; i++)
  {
    const std::string id = "id_" + std::to_string(i + 1);
    const std::string value(size, 'A');
    test_data.emplace(id, value);
  }

  for (const auto& item : test_data)
  {
    test.add_query(item.first, "127.0.0.1", std::to_string(server_port_1), item.second);
  }

  test.execute();

  const auto ids = test.get_ids();
  for (const auto& item : test_data)
  {
    if (ids.count(item.first) == 0)
    {
      TEST_FAILED("ID " + item.first + " is not found in result IDs");
    }
    else
    {
      const auto r = test[item.first];
      if (r.error_code)
      {
        TEST_FAILED("Request with ID " + item.first + " was expected to succeed");
      }

      if (r.body != item.second)
      {
        TEST_FAILED("Unexpected response for request with ID " + item.first);
      }
    }
  }

  if (ids.size() != test_data.size())
  {
    TEST_FAILED("Unexpected count of returned ID set");
  }

  TEST_PASSED();
}

int slow_server_read()
{
  SmartMet::Spine::TcpMultiQuery test(1);
  test.add_query("foo", "127.0.0.1", std::to_string(server_port_2), "foo");
  test.execute();
  const auto ids = test.get_ids();
  if (ids.size() != 1)
  {
    TEST_FAILED("Excactly 1 ID expected");
  }
  if (*ids.begin() != "foo")
  {
    TEST_FAILED("ID not as excpected");
  }

  const auto result = test["foo"];
  if (result.error_code != boost::system::errc::timed_out)
  {
    TEST_FAILED("Query was expected to be timed_out");
  }

  TEST_PASSED();
}

int two_requests_one_times_out()
{
  SmartMet::Spine::TcpMultiQuery test(1);
  test.add_query("foo", "127.0.0.1", std::to_string(server_port_1), "foo");
  test.add_query("bar", "127.0.0.1", std::to_string(server_port_2), "bar");
  test.execute();
  const auto ids = test.get_ids();
  if (ids.size() != 2)
  {
    TEST_FAILED("Excactly 2 ID expected");
  }

  const auto result = test["foo"];
  if (result.error_code)
  {
    TEST_FAILED("Query was expected to succeed");
  }

  if (result.error_desc != "")
  {
    TEST_FAILED("Error description expected to be empty - no error");
  }

  if (result.body != "foo")
  {
    TEST_FAILED("Response body is expected to be 'foo', but got '" + result.body + "'");
  }

  const auto result2 = test["bar"];
  if (result2.error_code != boost::system::errc::timed_out)
  {
    TEST_FAILED("Query was expected to be timed_out");
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * The actual test suite
 */
// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(empty);
    TEST(single_request);
    TEST(several_requests);
    TEST(slow_server_read);
    TEST(two_requests_one_times_out);
  }
};
};  // namespace TcpMultiQueryTest

int main(void)
{
  std::cout << std::endl
            << "TcpMultiQuery tester" << std::endl
            << "======================" << std::endl;
  a::io_context context;
  AsyncEchoServer::server echo_server(context, 0);
  AsyncEchoServer::server echo_server_slow_read(context, 2000);
  std::thread server_thread([&context]() { context.run(); });

  TcpMultiQueryTest::server_port_1 = echo_server.port();
  TcpMultiQueryTest::server_port_2 = echo_server_slow_read.port();

  int ret_code = 0;
  std::exception_ptr e;
  try
  {
    TcpMultiQueryTest::tests t;
    ret_code = t.run();
  }
  catch (...)
  {
    e = std::current_exception();
  }

  context.stop();
  server_thread.join();
  if (e)
  {
    std::rethrow_exception(e);
  }
  return ret_code;
}
