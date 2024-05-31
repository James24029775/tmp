#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#ifdef __linux__
#include <sys/wait.h>
#endif

#include "project3_util.h"
#include "panel.h"
#include "console_util.h"

std::string cgiPath, queryString;
std::vector<std::pair<std::string, std::string>> param;
extern std::vector<ClientInfo> clientInfo;

//---------------------- http server ----------------------//
void HttpSession::readRequest()
{
	auto self = shared_from_this();
	http::async_read(_socket, _buffer, _request, [this, self](auto error, auto)
					 {
		if (!error)
		{
			parseReq();
#ifdef __linux__
			forkCGI();
#else
			runCGI();
#endif
		} });
}

void HttpSession::parseReq()
{
	std::vector<std::string> strs;
	boost::split(strs, _request.target(), boost::is_any_of("?"));
	if (strs.size() < 2)
		strs.emplace_back("");

	auto httpHost = std::string(_request["Host"]);
	auto target = std::string(_request.target());
	auto requestMethod = std::string(_request.method_string());
	auto first = std::to_string(_request.version() / 10), second = std::to_string(_request.version() % 10);

	cgiPath = '.' + strs[0];
	queryString = strs[1];
	param.emplace_back("REQUEST_METHOD", requestMethod);
	param.emplace_back("REQUEST_URI", target);
	param.emplace_back("QUERY_STRING", strs[1]);
	param.emplace_back("SERVER_PROTOCOL", "HTTP/" + first + '.' + second);
	param.emplace_back("HTTP_HOST", httpHost);
	param.emplace_back("SERVER_ADDR", _socket.local_endpoint().address().to_string());
	param.emplace_back("SERVER_PORT", std::to_string(_socket.local_endpoint().port()));
	param.emplace_back("REMOTE_ADDR", _socket.remote_endpoint().address().to_string());
	param.emplace_back("REMOTE_PORT", std::to_string(_socket.remote_endpoint().port()));
}

#ifdef __linux__
void HttpSession::forkCGI()
{
	int pid;
	if ((pid = fork()) == 0)
	{
		// set_env
		for (const auto &e : param)
			setenv(e.first.c_str(), e.second.c_str(), OVERWRITE);

		// dup
		dup2(_socket.native_handle(), STDIN_FILENO);
		dup2(_socket.native_handle(), STDOUT_FILENO);
		dup2(_socket.native_handle(), STDERR_FILENO);
		close(_socket.native_handle());

		// exec
		std::cout << "HTTP/1.1 200 OK\r\n";
		fflush(stdout);
		std::vector<char *> args = {const_cast<char *>(cgiPath.c_str()), NULL};
		if (execv(cgiPath.c_str(), args.data()) == -1)
		{
			perror("execv");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		_socket.close();
		param.clear();
		waitpid(pid, NULL, 0);
	}
}
#endif

void HttpSession::runCGI()
{
	auto self = shared_from_this();
	if (cgiPath == "./panel.cgi")
	{
		auto _response = std::make_shared<http::response<http::string_body>>(http::status::ok, _request.version());
		_response->set(http::field::server, "NP");
		_response->set(http::field::content_type, "text/html");
		_response->keep_alive(_request.keep_alive());
		_response->body() = PANEL;
		_response->prepare_payload();
		http::async_write(_socket, *_response, [self, _response](auto, auto) {});
	}
	else if (cgiPath == "./console.cgi")
	{
		parseQureyFromEnv(queryString);
		std::string result = printHTML();
		_socket.async_write_some(asio::buffer(result), [self](auto error, auto)
								 {
		if (!error) {
		    boost::asio::ip::tcp::endpoint ep = self->_socket.remote_endpoint();
			for (size_t i = 0; i < clientNum; i++) {
				std::make_shared<clientSession>(i, ep, self->context, self)->start();
			}
		} });
	}
}

void HttpSession::write(std::string &data)
{
	bool isEmpty = _writebuf.empty();
	_writebuf.emplace(data);
	if (isEmpty)
		doWrite();
}

void HttpSession::doWrite()
{
	if (_writebuf.empty())
		return;
	auto self = shared_from_this();
	_socket.async_write_some(boost::asio::buffer(_writebuf.front()), [self](auto error, auto)
							 {
    if (!error) {
      self->_writebuf.pop();
      self->doWrite();
    } });
}
