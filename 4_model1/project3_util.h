#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <queue>

#ifndef PROJECT3_UTIL_H
#define PROJECT3_UTIL_H

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace asio = boost::asio;     // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

#define BUFFER_LENGTH 8096
#define OVERWRITE 1

//---------------------- http server ----------------------//
extern std::vector<std::pair<std::string, std::string>> param;

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
public:
    HttpSession(boost::asio::io_context *io_context, tcp::socket socket) : context(*io_context), _socket(std::move(socket)) {}
    void start() { readRequest(); }
    void write(std::string &data);
    void doWrite();
    void parseReq();
    void readRequest();
    void forkCGI();
    void runCGI();

private:
    boost::asio::io_context &context;
    tcp::socket _socket;
    std::queue<std::string> _writebuf;
    boost::beast::flat_buffer _buffer;
    boost::beast::http::request<boost::beast::http::string_body> _request;
};

class HttpServer
{
public:
    HttpServer(asio::io_context *context, short port)
        : io_context(*context), _acceptor(*context, tcp::endpoint(tcp::v4(), port))
    {
        acceptClient();
    }

private:
    boost::asio::io_context &io_context;
    tcp::acceptor _acceptor;
    void acceptClient()
    {
        _acceptor.async_accept(io_context, [this](boost::system::error_code ec, tcp::socket socket)
                               {
          		if (!ec){
            			std::make_shared<HttpSession>(&io_context, std::move(socket))->start();
          		}
          		acceptClient(); });
    }
};

#endif
