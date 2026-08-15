#include <random>
#include <functional>
#include <boost/asio/io_context.hpp>
#include <fmt/base.h>
#include "chrony.h"
#include "chrony_printers.h"

using namespace std::chrono;
using chrony_client = chrony::basic_chrony_client<boost::asio::io_context::executor_type>;
using chrony::payload_tracking;
using chrony::payload_source_data;

void print_chrony_all(chrony_client& sock)
{
    sock.async_read_tracking([&](auto ec, const payload_tracking& pay) {
        if (ec) {
            fmt::println(stderr, "Error: {}", ec.message());
            return;
        }
        
        print(pay);
        sock.async_read_sources([](auto ec, const std::vector<payload_source_data>& sources) {
            if (ec) {
                fmt::println(stderr, "Error: {}", ec.message());
                return;
            }
            print(sources);
        });
    });
}

int main()
{
    boost::asio::io_context ioc{1};
    chrony_client sock(ioc);
    print_chrony_all(sock);
    ioc.run();
}