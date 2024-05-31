#include <iostream>
#include <stdlib.h>

#include "util.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " [port]" << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        asio::io_context io_context;
        Socks4Server sock4Server(&io_context, std::atoi(argv[1]));
        io_context.run();
    }
    catch (const std::exception &e)
    {
        return EXIT_FAILURE;
    }
}
