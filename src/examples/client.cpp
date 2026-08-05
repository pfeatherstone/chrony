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

std::string format_address(const chrony_address& address)
{
    if (const auto ip = address.to_address())
        return ip->to_string();

    if (const auto id = address.to_id())
    {
        std::ostringstream stream;
        stream << "ID:" << std::hex << std::uppercase
               << std::setw(8) << std::setfill('0') << *id;
        return stream.str();
    }

    return "<unspecified>";
}

awaitable<void> print_chrony_tracking(chrony_client<boost::asio::io_context::executor_type>& sock)
{
    try
    {
        const payload_tracking pay = co_await sock.async_read_tracking();

        std::cout << "Tracking results:\n";
        std::cout << "  reference_id        : " << std::hex << std::uppercase << pay.reference_id << std::dec << '\n';
        std::cout << "  address             : " << format_address(pay.address) << '\n';
        std::cout << "  stratum             : " << pay.stratum << '\n';
        std::cout << "  leap_status         : " << to_string(pay.status) << '\n';
        std::cout << "  ref_time            : " << pay.ref_time.to_time_point() << '\n';
        std::cout << "  current_correction  : " << pay.current_correction.to_double() << '\n';
        std::cout << "  last_offset         : " << pay.last_offset.to_double() << '\n';
        std::cout << "  rms_offset          : " << pay.rms_offset.to_double() << '\n';
        std::cout << "  freq_offset_ppm     : " << pay.freq_offset_ppm.to_double() << '\n';
        std::cout << "  freq_residual_ppm   : " << pay.freq_residual_ppm.to_double() << '\n';
        std::cout << "  skew_ppm            : " << pay.skew_ppm.to_double() << '\n';
        std::cout << "  root_delay          : " << pay.root_delay.to_double() << '\n';
        std::cout << "  root_dispersion     : " << pay.root_dispersion.to_double() << '\n';
        std::cout << "  last_update_interval: " << pay.last_update_interval.to_double() << '\n';
        std::cout << '\n';
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error : " << e.what() << '\n';
    }
}

awaitable<void> print_chrony_sources(chrony_client<boost::asio::io_context::executor_type>& sock)
{
    try
    {
        const std::vector<payload_source_data> sources = co_await sock.async_read_sources();
        std::cout
            << "Chrony sources: " << sources.size() << "\n\n"
            << std::left
            << std::setw(3)  << "St"
            << std::setw(40) << "Address"
            << std::setw(10) << "Mode"
            << std::setw(8)  << "Stratum"
            << std::setw(10) << "Poll(s)"
            << std::setw(8)  << "Reach"
            << std::setw(10) << "Age(s)"
            << std::setw(16) << "Offset(s)"
            << std::setw(16) << "Original(s)"
            << "Error(s)"
            << '\n';

        std::cout << std::string(140, '-') << '\n';

        for (const auto& source : sources)
        {
            const double poll_seconds = std::ldexp(1.0, source.poll);

            std::ostringstream reachability;
            reachability << std::oct << source.reachability;

            std::cout
                << std::left
                << std::setw(3)
                << state_symbol(source.state)

                << std::setw(40)
                << format_address(source.address)

                << std::setw(10)
                << to_string(source.mode)

                << std::setw(8)
                << source.stratum

                << std::setw(10)
                << poll_seconds

                << std::setw(8)
                << reachability.str()

                << std::setw(10)
                << source.since_sample

                << std::scientific
                << std::setprecision(6)

                << std::setw(16)
                << source.adjusted_measurement.to_double()

                << std::setw(16)
                << source.original_measurement.to_double()

                << source.measurement_error.to_double()

                << std::defaultfloat
                << '\n';
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error : " << e.what() << '\n';
    }
}

awaitable<void> print_chrony_all(chrony_client<boost::asio::io_context::executor_type>& sock)
{
    co_await print_chrony_tracking(sock);
    co_await print_chrony_sources(sock);
}

int main()
{
    boost::asio::io_context ioc{1};
    chrony_client<boost::asio::io_context::executor_type> sock(ioc);
    co_spawn(ioc, print_chrony_all(sock), detached);
    ioc.run();
}