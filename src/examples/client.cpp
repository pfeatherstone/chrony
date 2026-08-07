#include <random>
#include <functional>
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <fmt/chrono.h>
#include <fmt/base.h>
#include "chrony.h"

using namespace std::chrono;
using boost::asio::detached;
using awaitable     = boost::asio::awaitable<void, boost::asio::io_context::executor_type>;
using chrony_client = chrony::basic_chrony_client<boost::asio::io_context::executor_type>;
using chrony::payload_tracking;
using chrony::payload_source_data;

std::string format_address(const chrony::chrony_address& address)
{
    if (const auto ip = address.to_address()) return ip->to_string();
    if (const auto id = address.to_id())      return fmt::format("ID:{:08X}", *id);
    else                                      return "<unspecified>";
}

awaitable print_chrony_tracking(chrony_client& sock)
{
    try
    {
        const payload_tracking pay = co_await sock.async_read_tracking();

        fmt::println("Tracking results:");
        fmt::println("  reference_id        : {:08X}", pay.reference_id);
        fmt::println("  address             : {}", format_address(pay.address));
        fmt::println("  stratum             : {}", pay.stratum);
        fmt::println("  leap_status         : {}", to_string(pay.status));
        fmt::println("  ref_time            : {}", pay.ref_time.to_time_point());
        fmt::println("  current_correction  : {}", pay.current_correction.to_double());
        fmt::println("  last_offset         : {}", pay.last_offset.to_double());
        fmt::println("  rms_offset          : {}", pay.rms_offset.to_double());
        fmt::println("  freq_offset_ppm     : {}", pay.freq_offset_ppm.to_double());
        fmt::println("  freq_residual_ppm   : {}", pay.freq_residual_ppm.to_double());
        fmt::println("  skew_ppm            : {}", pay.skew_ppm.to_double());
        fmt::println("  root_delay          : {}", pay.root_delay.to_double());
        fmt::println("  root_dispersion     : {}", pay.root_dispersion.to_double());
        fmt::println("  last_update_interval: {}", pay.last_update_interval.to_double());
        fmt::println("");
    }
    catch(const std::exception& e)
    {
        fmt::println(stderr, "Error: {}", e.what());
    }
}

awaitable print_chrony_sources(chrony_client& sock)
{
    try
    {
        const std::vector<payload_source_data> sources = co_await sock.async_read_sources();
        
        fmt::println("Chrony sources: {}\n", sources.size());

        fmt::println("{:<3} {:<20} {:<9} {:>7} {:>9} {:>7} {:>9} {:>10} {:>12} {:>14}",
            "St", "Address", "Mode", "Stratum", "Poll(s)", "Reach", "Age(s)", "Offset(s)", "Original(s)", "Error(s)");

        for (const auto& source : sources)
        {
            fmt::println("{:<3} {:<20} {:<9} {:>7} {:>9} {:>7o} {:>9} {:+8}us [{:+8}us] +/- {:>8}us",
                state_symbol(source.state),
                format_address(source.address),
                to_string(source.mode),
                source.stratum,
                duration_cast<seconds>(source.poll()).count(),
                source.reachability,
                source.since_sample,
                static_cast<int64_t>(source.adjusted_measurement.to_double()*1e6),
                static_cast<int64_t>(source.original_measurement.to_double()*1e6),
                static_cast<int64_t>(source.measurement_error.to_double()*1e6)
            );
        }
    }
    catch (const std::exception& e)
    {
        fmt::println(stderr, "Error: {}", e.what());
    }
}

awaitable print_chrony_all(chrony_client& sock)
{
    co_await print_chrony_tracking(sock);
    co_await print_chrony_sources(sock);
}

int main()
{
    boost::asio::io_context ioc{1};
    chrony_client sock(ioc);
    co_spawn(ioc, print_chrony_all(sock), detached);
    ioc.run();
}