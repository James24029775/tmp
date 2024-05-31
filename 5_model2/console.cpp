#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

#include "console_util.h"

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace asio = boost::asio;     // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>


int main(int argc, char *argv[])
{
    asio::io_context io_context;
    parseQureyFromEnv(getenv("QUERY_STRING"));
    printHTML();
    printConsole(io_context);
    io_context.run();
}
