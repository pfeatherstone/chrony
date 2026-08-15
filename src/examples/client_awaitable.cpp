#include <random>
#include <functional>
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <fmt/base.h>
#include "chrony.h"
#include "chrony_printers.h"

using namespace std::chrono;
using boost::asio::detached;
using boost::asio::co_spawn;
using awaitable     = boost::asio::awaitable<void, boost::asio::io_context::executor_type>;
using chrony_client = chrony::basic_chrony_client<boost::asio::io_context::executor_type>;
using chrony::payload_tracking;
using chrony::payload_source_data;
using chrony::payload_sourcestats;

awaitable print_chrony_all(chrony_client& sock)
{
    try
    {
        const payload_tracking pay = co_await sock.async_read_tracking();
        print(pay);
        const std::vector<payload_source_data> sources = co_await sock.async_read_sources();
        print(sources);
        const std::vector<payload_sourcestats> stats = co_await sock.async_read_sourcestats();
        print(stats);
    }
    catch(const std::exception& e)
    {
        fmt::println(stderr, "Error: {}", e.what());
    }
}

int main()
{
    boost::asio::io_context ioc{1};
    chrony_client sock(ioc);
    co_spawn(ioc, print_chrony_all(sock), detached);
    ioc.run();
}