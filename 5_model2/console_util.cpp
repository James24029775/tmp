#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <regex>

#include "console_util.h"
#include "util.h"

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace asio = boost::asio;     // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

std::vector<ClientInfo> clientInfo(6);

std::string clientSession::outputConsole()
{
    std::string result = "";
    result += "<script>document.getElementById('s" + std::to_string(index) + "').innerHTML += '";
    result += getEscape(std::string(_data));
    result += "';</script>\n";
    std::cout << result;

    return result;
}

std::string clientSession::outputCommand(std::string &command)
{
    std::string result = "";
    result += "<script>document.getElementById('s" + std::to_string(index) + "').innerHTML += '<b>";
    result += getEscape(command);
    result += "</b>';</script>";
    std::cout << result << std::endl;
    result += '\n';

    return result;
}

void clientSession::writeResponse()
{
    auto self = shared_from_this();
    std::string command;
    std::getline(_file, command);
    command += "\n";
    if (command.find("exit") != std::string::npos)
    {
        _file.close();
        clientInfo[index].host = "NULL";
    }
    outputCommand(command);

    // server may have latency to give the result
    async_write(_socket, asio::buffer(command.c_str(), command.length()), [this, self](auto error, size_t)
                {
            if ( !error ) {
                if (clientInfo[index].host != "NULL")
                    readRequest();
            } });
}

std::string clientSession::getEscape(std::string data)
{
    std::string result = "";
    for (char c : data)
    {
        switch (c)
        {
        case '\n':
            result += "<br>";
            break;
        case '\r':
            result += "";
            break;
        case '&':
            result += "&amp;";
            break;
        case '\"':
            result += "&quot;";
            break;
        case '\'':
            result += "&apos;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        default:
            result += c;
            break;
        }
    }
    return result;
}

void clientSession::readRequest()
{
    auto self = shared_from_this();
    memset(_data, 0, BUFFER_LENGTH);
    _socket.async_read_some(asio::buffer(_data, BUFFER_LENGTH), [this, self](auto error, size_t length) // receive data from np shell
                            {
        {
            if (!error)
            {
                std::string content = outputConsole();

                /* if "% " exists, then read test case, and write the test case to console and np shell */
                if (content.find("% ") != std::string::npos) 
                    writeResponse();
                /* else, keep receving */
                else
                    readRequest(); 
            }
        } });
}

void clientSession::windowsSession()
{
    auto self = shared_from_this();
    memset(_data, 0, BUFFER_LENGTH);
    _socket.async_read_some(asio::buffer(_data, BUFFER_LENGTH), [this, self](auto error, size_t length) // receive data from np shell
                            {
        {
            if (!error)
            {
                auto content = outputConsole();
                self->_session->write(content);

                /* if "% " exists, then read test case, and write the test case to console and np shell */
                if (content.find("% ") != std::string::npos)
                {
                    std::string command;
                    if (std::getline(self->_file, command))
                    {
                        command += '\n';
                        content = outputCommand(command);
                        self->_session->write(content);

                        self->_socket.async_write_some(asio::buffer(command), [self](auto, auto)
                                                        { self->windowsSession(); });
                    }
                }
                /* else, keep receving */
                else
                    self->windowsSession();
                }
            } });
}

void clientSession::acceptClient(tcp::resolver::iterator it)
{
    auto self = shared_from_this();
    _socket.async_connect(*it, [self](auto error)
                          {
        if ( !error ) {
            if (self->_session){
                self->windowsSession();
            }
            else{
                self->readRequest();
            }
        } });
}

void clientSession::resolveService()
{
    char buffer[12] = {4, 1, 0, 0, 0, 0, 0, 0, 'H', 'W', '4', 0};
    auto &&socks4Header = *reinterpret_cast<Socks4Header *>(buffer);

    tcp::endpoint targetEP = *_resolver.resolve(tcp::v4(), clientInfo[index].host, clientInfo[index].port);
    socks4Header.dstPort = htons(targetEP.port());
    socks4Header.dstIp = htonl(targetEP.address().to_v4().to_ulong());
    _socket.connect(_sock4);
    _socket.write_some(asio::buffer(buffer, sizeof(buffer)));
    _socket.read_some(asio::buffer(buffer, sizeof(Socks4Header)));
    if (socks4Header.cd == 90)
        readRequest();
}

void printConsole(asio::io_context &io_context)
{
    boost::asio::ip::tcp::resolver resolver(io_context);
    auto &&socks4 = clientInfo[clientInfo.size() - 1];
    boost::asio::ip::tcp::endpoint ep = *resolver.resolve(socks4.host, socks4.port);
    std::cout << ep << std::endl;

    for (size_t i = 0; i < clientNum; i++)
        if (clientInfo[i].host != "NULL")
            std::make_shared<clientSession>(i, ep, io_context)->start();
}

std::string printHTML()
{
    std::string result = "";
    result += HTTP200;
    result += HTML_HEADER;
    result += HTML_CONTENT[0];

    for (size_t i = 0; i < clientNum; i++)
        if (clientInfo[i].host != "NULL")
            result += R"(<th scope="col">)" + clientInfo[i].host + ':' + clientInfo[i].port + "</th>\n";
    result += HTML_CONTENT[1];
    for (size_t i = 0; i < clientNum; i++)
        if (clientInfo[i].host != "NULL")
            result += R"(<td><pre id="s)" + std::to_string(i) + R"(" class="mb-0"></pre></td>)" + '\n';
    result += HTML_CONTENT[2];

    std::cout << result;
    return result;
}

void parseQureyFromEnv(std::string query)
{
    std::vector<std::string>
        _long,
        _short;
    boost::split(_long, query, boost::is_any_of("&"));
    for (size_t i = 0; i < clientNum; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            boost::split(_short, _long[i * 3 + j], boost::is_any_of("="));

            if (_short[1] == "")
            {
                clientInfo[i].host = "NULL";
                continue;
            }
            else
            {
                if (j == 0)
                    clientInfo[i].host = _short[1];
                else if (j == 1)
                    clientInfo[i].port = _short[1];
                else if (j == 2)
                    clientInfo[i].file = _short[1];
            }
        }
    }

    int last = clientInfo.size() - 1;
    boost::split(_long, query, boost::is_any_of("&"));
    for (int j = 0; j < 2; j++)
    {
        boost::split(_short, _long[last * 3 + j], boost::is_any_of("="));

        if (_short[1] == "")
        {
            clientInfo[last].host = "NULL";
            continue;
        }
        else
        {
            if (j == 0)
                clientInfo[last].host = _short[1];
            else if (j == 1)
                clientInfo[last].port = _short[1];
        }
    }
}

std::string HTTP200 = "HTTP/1.1 200 OK\r\n";
std::string HTML_HEADER = "Content-type: text/html\r\n\r\n";
std::string HTML_CONTENT[] = {
    R"(<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>NP Project 3 Sample Console</title>
    <link
      rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
        font-size: 1rem !important;
      }
      body {
        background-color: #212529;
      }
      pre {
        color: #cccccc;
      }
      b {
        color: #01b468;
      }
    </style>
  </head>
  <body>
    <table class="table table-dark table-bordered">
      <thead>
        <tr>)",
    R"(</tr>
      </thead>
      <tbody>
        <tr>)",
    R"(</tr>
      </tbody>
    </table>
  </body>
</html>))"};
