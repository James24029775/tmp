#include <iostream>
#include <iomanip>
#include <regex>
#include <fstream>

#include "util.h"

Socks4Server::Socks4Server(asio::io_context *context, int port) : _context(*context), _acceptor(*context, tcp::endpoint(tcp::v4(), port)), _socket(*context)
{
    run();
}

void Socks4Server::run()
{
    _acceptor.async_accept(_context, [this](auto ec, tcp::socket socket)
                           {
            if (!ec){
                _context.notify_fork(asio::io_service::fork_prepare);
                pid_t pid = fork();
                if (pid == -1)
                {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }
                else if (pid == 0)
                {
                    _context.notify_fork(asio::io_service::fork_child);
                    std::make_shared<Socks4Handler>(&_context, std::move(socket))->run();
                }
                else 
                {
                    _context.notify_fork(asio::io_service::fork_parent);
                    run();
                }
            } });
}

Socks4Handler::Socks4Handler(asio::io_context *context, asio::ip::tcp::socket socket) : _context(*context), _browsorSocket(std::move(socket)), _serverSocket(*context), _resolver(*context), _acceptor(*context), _bindAcceptor(*context) {}

void Socks4Handler::run()
{
    auto self = shared_from_this();
    _browsorSocket.async_read_some(boost::asio::buffer(_browsor_data), [self](const auto &error, std::size_t bytes_transferred)
                                   {
    if (!error)
    {
        std::regex socks4a(R"(0\.0\.0\.[1-9][0-9]{0,2})", std::regex_constants::optimize);
        Socks4Header socks4Header;
        memcpy(&socks4Header, self->_browsor_data.data(), sizeof(socks4Header));
        asio::ip::address_v4 addr(ntohl(socks4Header.dstIp));
        asio::ip::tcp::endpoint serverEndpoint;

        if (std::regex_match(addr.to_string(), socks4a)) {
            std::string userid(self->_browsor_data.data() + sizeof(Socks4Header));
            std::string domainName(self->_browsor_data.data() + sizeof(Socks4Header) + userid.length() + 1);
            serverEndpoint = *self->_resolver.resolve(tcp::v4(), domainName, std::to_string(ntohs(socks4Header.dstPort)));
        } else {
            serverEndpoint.address(addr);
            serverEndpoint.port(ntohs(socks4Header.dstPort));
        }

        auto browsorEndpoint = self->_browsorSocket.remote_endpoint();
        bool isConnect = (socks4Header.cd == 1);
        std::cout << "<S_IP>: " << browsorEndpoint.address() << std::endl;
        std::cout << "<S_PORT>: " << browsorEndpoint.port() << std::endl;
        std::cout << "<D_IP>: " << serverEndpoint.address() << std::endl;
        std::cout << "<D_PORT>: " << serverEndpoint.port() << std::endl;
        std::cout << "<Command>: " << (isConnect ? "CONNECT" : "BIND") << std::endl;

        if (!checkFirewall(serverEndpoint.address().to_string(), socks4Header.cd)) {
            std::cout << "<Reply>: Reject" << std::endl;
            self->reject();
            return;
        } else 
            std::cout << "<Reply>: Accept" << std::endl;

        if (socks4Header.cd == CONNECT)
            self->connect(serverEndpoint);
        else if (socks4Header.cd == BIND)
            self->bind();
    } });
}

void Socks4Handler::connect(asio::ip::tcp::endpoint &serverEndpoint)
{
    // connect to the remote server
    auto self = shared_from_this();
    _serverSocket.async_connect(serverEndpoint,
                                [self](const auto &error)
                                {
                                    if (!error)
                                    {
                                        self->accept(); // send SOCKS4 ACCEPT to browsor
                                        self->readRemote();
                                        self->writeRemote();
                                    }
                                    else
                                    {
                                        self->reject(); // send SOCKS4 REJECT to browsor
                                        return;
                                    }
                                });
}

void Socks4Handler::accept(uint16_t port)
{
    Socks4Header sockes4Header{};
    sockes4Header.vn = 0;
    sockes4Header.cd = ACCEPT;
    sockes4Header.dstPort = htons(port);
    sockes4Header.dstIp = 0;
    _browsorSocket.async_write_some(asio::buffer(&sockes4Header, sizeof(Socks4Header)), [](const auto &error, std::size_t) {});
}

void Socks4Handler::reject()
{
    Socks4Header sockes4Header{};
    sockes4Header.vn = 0;
    sockes4Header.cd = REJECT;
    sockes4Header.dstPort = 0;
    sockes4Header.dstIp = 0;
    _browsorSocket.async_write_some(asio::buffer(&sockes4Header, sizeof(Socks4Header)), [](const auto &error, std::size_t) {});
}

void Socks4Handler::readRemote()
{
    auto self = shared_from_this();
    _serverSocket.async_wait(asio::socket_base::wait_read, [self](const auto &error)
                             {
    if (!error) {
        boost::system::error_code ec;
        auto readCount = self->_serverSocket.read_some(asio::buffer(self->_server_data), ec);

        self->limit += readCount;
        std::cout << "readCount: " << readCount << std::endl;
        std::cout << "self->limit: " << self->limit << std::endl;
        if (ec || self->limit >= LIMIT) {
            self->_browsorSocket.shutdown(tcp::socket::shutdown_send);
            self->limit = 0;
            return;
        } else {
            self->_browsorSocket.write_some(asio::buffer(self->_server_data, readCount));
            self->readRemote();
        }
    } });
}

void Socks4Handler::writeRemote()
{
    auto self = shared_from_this();
    _browsorSocket.async_wait(asio::socket_base::wait_read, [self](const auto &error)
                              {
    if (!error) {
        boost::system::error_code ec;
        auto readCount = self->_browsorSocket.read_some(asio::buffer(self->_browsor_data), ec);
        if (ec) return;
        self->_serverSocket.write_some(asio::buffer(self->_browsor_data, readCount));
        self->writeRemote();
    } });
}

void Socks4Handler::bind()
{
    // initiate a random port
    asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), 0);
    _bindAcceptor.open(tcp::v4());
    _bindAcceptor.bind(ep);
    _bindAcceptor.listen(1);

    // build the session between browsor and server
    auto port = _bindAcceptor.local_endpoint().port();
    accept(port);
    _serverSocket = _bindAcceptor.accept();
    accept(port);

    readRemote();
    writeRemote();
}

bool checkFirewall(const std::string &ip, uint8_t command)
{
    std::ifstream configFile("socks.conf");
    if (!configFile.is_open())
        return false;

    std::string line;
    while (std::getline(configFile, line))
    {
        std::istringstream iss(line);
        std::string ipPattern;
        char connectionType;
        if (!(iss >> ipPattern >> connectionType >> ipPattern))
            continue;

        if ((command == CONNECT && connectionType == 'c') || (command == BIND && connectionType == 'b'))
        {
            generateRE(ipPattern); // \..*\..*\..*\..* -like string
            std::regex ruleRegex(ipPattern);
            if (std::regex_match(ip, ruleRegex))
                return true;
        }
    }
    return false;
}

void generateRE(std::string &str)
{
    std::string from, to;
    size_t start_pos;

    from = ".";
    to = "\\.";
    start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }

    from = "*";
    to = ".*";
    start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}
