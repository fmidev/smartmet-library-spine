// ======================================================================
/*!
 * \file
 * \brief Regression tests for class TcpMultiQuery
 */
// ======================================================================

#include "TcpMultiQuery.h"
#include <regression/tframe.h>
#include <memory>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread.hpp>

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

    class session
    {
    public:
        session(boost::asio::io_context& io_context)
            : socket_(io_context)
        {
        }

        tcp::socket& socket()
        {
            return socket_;
        }

        void start()
        {
            socket_.async_read_some(boost::asio::buffer(data_, max_length),
                boost::bind(&session::handle_read, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }

    private:
        void handle_read(const boost::system::error_code& error,
            size_t bytes_transferred)
        {
            if (!error)
            {
                boost::asio::async_write(socket_,
                    boost::asio::buffer(data_, bytes_transferred),
                    boost::bind(&session::handle_write, this,
                        boost::asio::placeholders::error));
            }
            else
            {
                delete this;
            }
        }

        void handle_write(const boost::system::error_code& error)
        {
            if (!error)
            {
                socket_.async_read_some(boost::asio::buffer(data_, max_length),
                    boost::bind(&session::handle_read, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
            }
            else
            {
                delete this;
            }
        }

        tcp::socket socket_;
        enum { max_length = 1024 };
        char data_[max_length];
    };

    class server
    {
        boost::asio::io_context& io_context_;
        tcp::acceptor acceptor_;
    public:
        server(boost::asio::io_context& io_context)
            : io_context_(io_context),
              acceptor_(io_context, tcp::endpoint(tcp::v4(), 0))
        {
            start_accept();
        }

        int port() const { return acceptor_.local_endpoint().port(); }

    private:
        void start_accept()
        {
            session* new_session = new session(io_context_);
            acceptor_.async_accept(new_session->socket(),
                boost::bind(&server::handle_accept, this, new_session,
                    boost::asio::placeholders::error));
        }

        void handle_accept(session* new_session,
            const boost::system::error_code& error)
        {
            if (!error)
            {
                new_session->start();
            }
            else
            {
                delete new_session;
            }

            start_accept();
        }

    };

};

namespace TcpMultiQueryTest
{
    int server_port = 0;

    void empty()
    {
        SmartMet::Spine::TcpMultiQuery test(1);
        test.execute();
        if (not test.get_ids().empty()) {
            TEST_FAILED("No request IDs provided");
        }
        TEST_PASSED();
    }

    int single_request()
    {
        SmartMet::Spine::TcpMultiQuery test(1);
        test.add_query("foo", "127.0.0.1", std::to_string(server_port), "foo");
        test.execute();
        const auto ids = test.get_ids();
        if (ids.size() != 1) {
            TEST_FAILED("Excactly 1 ID expected");
        }
        if (*ids.begin() != "foo") {
            TEST_FAILED("ID not as excpected");
        }

        const auto result = test["foo"];
        if (result.error_code) {
            TEST_FAILED("Query was expected to succeed");
        }

        if (result.error_desc != "") {
            TEST_FAILED("Error description expected to be empty - no error");
        }

        if (result.body != "foo") {
            TEST_FAILED("Response body is expected to be 'foo', but got '"
                + result.body + "'");
        }

        TEST_PASSED();
    }

    int several_requests()
    {
        SmartMet::Spine::TcpMultiQuery test(1);
        std::map<std::string, std::string> test_data;
        int size = 1;
        for (int i = 0; i < 16; i++) {
            const std::string id = "id_" + std::to_string(i+1);
            const std::string value(size, 'A');
            test_data.emplace(id, value);
        }

        for (const auto& item : test_data) {
            test.add_query(item.first, "127.0.0.1", std::to_string(server_port), item.second);
        }

        test.execute();

        const auto ids = test.get_ids();
        for (const auto& item : test_data) {
            if (ids.count(item.first) == 0) {
                TEST_FAILED("ID " + item.first + " is not found in result IDs");
            } else {
                const auto r = test[item.first];
                if (r.error_code) {
                    TEST_FAILED("Request with ID " + item.first + " was expected to succeed");
                }

                if (r.body != item.second) {
                    TEST_FAILED("Unexpected response for request with ID " + item.first);
                }
            }
        }

        if (ids.size() != test_data.size()) {
            TEST_FAILED("Unexpected count of returned ID set");
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
        }
    };
};

int main(void)
{
    std::cout << std::endl
              << "TcpMultiQuery tester" << std::endl
              << "======================" << std::endl;
    a::io_context context;
    AsyncEchoServer::server echo_server(context);
    std::thread server_thread([&context]() { context.run(); });

    TcpMultiQueryTest::server_port = echo_server.port();
    std::cout << "Server port: " << TcpMultiQueryTest::server_port << std::endl;

    int ret_code = 0;
    std::exception_ptr e;
    try {
        TcpMultiQueryTest::tests t;
        ret_code = t.run();
    } catch (...) {
        e = std::current_exception();
    }

    context.stop();
    server_thread.join();
    if (e) {
        std::rethrow_exception(e);
    }
    return ret_code;
}
