#include <random>
#include <functional>
#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include "chrony.h"

using namespace chrony;
using boost::asio::detached;
template<class T> using awaitable = boost::asio::awaitable<T, boost::asio::io_context::executor_type>;

awaitable<void> print_chrony_tracking(chrony_client<boost::asio::io_context::executor_type>& sock)
{
    try
    {
        const payload_tracking pay = co_await sock.async_read_tracking();

        std::cout << "reference_id        : " << std::hex << std::uppercase << pay.reference_id << std::dec << '\n';
        if      (pay.address.is_ip()) std::cout << "address             : " << pay.address.to_address().value().to_string() << '\n';
        else if (pay.address.is_id()) std::cout << "address             : " << pay.address.to_id().value() << '\n';
        std::cout << "stratum             : " << pay.stratum << '\n';
        std::cout << "leap_status         : " << to_string(pay.status) << '\n';
        std::cout << "ref_time            : " << pay.ref_time.to_time_point() << '\n';
        std::cout << "current_correction  : " << pay.current_correction.to_double() << '\n';
        std::cout << "last_offset         : " << pay.last_offset.to_double() << '\n';
        std::cout << "rms_offset          : " << pay.rms_offset.to_double() << '\n';
        std::cout << "freq_offset_ppm     : " << pay.freq_offset_ppm.to_double() << '\n';
        std::cout << "freq_residual_ppm   : " << pay.freq_residual_ppm.to_double() << '\n';
        std::cout << "skew_ppm            : " << pay.skew_ppm.to_double() << '\n';
        std::cout << "root_delay          : " << pay.root_delay.to_double() << '\n';
        std::cout << "root_dispersion     : " << pay.root_dispersion.to_double() << '\n';
        std::cout << "last_update_interval: " << pay.last_update_interval.to_double() << '\n';
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error : " << e.what() << '\n';
    }
}

int main()
{
    boost::asio::io_context ioc{1};
    chrony_client<boost::asio::io_context::executor_type> sock(ioc);
    co_spawn(ioc, print_chrony_tracking(sock), detached);
    ioc.run();
}