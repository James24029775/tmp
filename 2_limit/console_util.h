#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#include "project3_util.h"

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace asio = boost::asio;     // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
#define BUFFER_LENGTH 8096
#define clientNum 5

class ClientInfo
{
public:
  std::string host;
  std::string port;
  std::string file;
};

extern std::vector<ClientInfo> clientInfo;
extern std::string HTTP200;
extern std::string HTML_HEADER;
extern std::string HTML_CONTENT[];

class clientSession : public std::enable_shared_from_this<clientSession>
{
public:
  clientSession(size_t i, boost::asio::ip::tcp::endpoint sock4, boost::asio::io_context &context, std::shared_ptr<HttpSession> s = nullptr) : index(i), _sock4(sock4), _session(s), _resolver(context), _socket(context)
  {
    _file.open(("./test_case/" + clientInfo[index].file), std::ios::in);
  };
  void start()
  {
    resolveService();
  };

private:
  size_t index;
  boost::asio::ip::tcp::endpoint _sock4;
  std::shared_ptr<HttpSession> _session;
  tcp::resolver _resolver;
  tcp::socket _socket;
  std::string _buffer;
  boost::beast::http::request<boost::beast::http::string_body> _request;
  void resolveService();
  void acceptClient(tcp::resolver::iterator it);
  void readRequest();
  void writeResponse();
  void windowsSession();
  std::string outputConsole();                     // output result to console
  std::string outputCommand(std::string &command); // output command to shell
  std::string getEscape(std::string data);

  char _data[BUFFER_LENGTH];
  std::fstream _file;
};

void parseQureyFromEnv(std::string query);
std::string printHTML();
void printConsole(asio::io_context &io_contex);
