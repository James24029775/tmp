#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <unordered_set>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace asio = boost::asio;     // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

#define CONNECT 1
#define BIND 2
#define ACCEPT 90
#define REJECT 91

class Socks4Server : public std::enable_shared_from_this<Socks4Server>
{
public:
    Socks4Server(asio::io_context *context, int port);
    void run();

private:
    std::unordered_set<std::string> ipExist;
    asio::io_context &_context;
    asio::ip::tcp::acceptor _acceptor;
    asio::ip::tcp::socket _socket;
};

class Socks4Header
{
public:
    uint8_t vn;
    uint8_t cd;
    uint16_t dstPort;
    uint32_t dstIp;
};

class Socks4Handler : public std::enable_shared_from_this<Socks4Handler>
{
public:
    Socks4Handler(asio::io_context *context, asio::ip::tcp::socket socket);
    void run();
    void connect(asio::ip::tcp::endpoint &serverEndpoint);
    void accept(uint16_t port = 0);
    void reject();
    void readRemote();
    void writeRemote();
    void bind();

private:
    asio::io_context &_context;
    asio::ip::tcp::socket _browsorSocket;
    asio::ip::tcp::socket _serverSocket;
    asio::ip::tcp::resolver _resolver;
    asio::ip::tcp::acceptor _acceptor;
    asio::ip::tcp::acceptor _bindAcceptor;
    std::array<char, 4096> _browsor_data;
    std::array<char, 4096> _server_data;
};

bool checkFirewall(const std::string &ip, uint8_t command);

void generateRE(std::string &str);
