#include <random>
#include <functional>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/detached.hpp>
#include <fmt/base.h>
#include "chrony.h"
#include "chrony_printers.h"

using namespace std::chrono;
using boost::asio::detached;
using boost::asio::spawn;
using yield_context = boost::asio::basic_yield_context<boost::asio::io_context::executor_type>;
using chrony_client = chrony::basic_chrony_client<boost::asio::io_context::executor_type>;
using chrony::payload_tracking;
using chrony::payload_source_data;

void print_chrony_all(chrony_client& sock, yield_context yield)
{
    try
    {
        const payload_tracking pay = sock.async_read_tracking(yield);
        print(pay);
        const std::vector<payload_source_data> sources = sock.async_read_sources(yield);
        print(sources);
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
    spawn(ioc, std::bind_front(print_chrony_all, std::ref(sock)), detached);
    ioc.run();
}