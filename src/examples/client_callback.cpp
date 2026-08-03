#include <random>
#include <functional>
#include <boost/asio/io_context.hpp>
#include <fmt/base.h>
#include <fmt/chrono.h>
#include "chrony.h"

using namespace chrony;

struct chrony_session
{
    chrony_client<boost::asio::io_context::executor_type> sock;
    
    chrony_session (
        boost::asio::io_context& ioc,
        const char* chronyd_sock_path
    ) : sock(ioc.get_executor())
    {
    }

    void start()
    {
        send_tracking_request();
    }

    void send_tracking_request()
    {
        async_read_tracking(sock, std::bind_front(&chrony_session::on_tracking, this));
    }

    void on_tracking(boost::system::error_code ec, const payload_tracking& pay)
    {
        if (ec)
        {
            printf("on_tracking() error : %s\n", ec.message().c_str());
            return;
        }

        fmt::println("reference_id        : {}", pay.reference_id);
        // fmt::println("address             : {}", pay.address);
        fmt::println("stratum             : {}", pay.stratum);
        fmt::println("leap_status         : {}", to_string(pay.status));
        fmt::println("ref_time            : {}", pay.ref_time.to_time_point());
        fmt::println("current_correction  : {}", pay.current_correction.to_double());
        fmt::println("last_offset         : {}", pay.last_offset.to_double());
        fmt::println("rms_offset          : {}", pay.rms_offset.to_double());
        fmt::println("freq_offset_ppm     : {}", pay.freq_offset_ppm.to_double());
        fmt::println("freq_residual_ppm   : {}", pay.freq_residual_ppm.to_double());
        fmt::println("skew_ppm            : {}", pay.skew_ppm.to_double());
        fmt::println("root_delay          : {}", pay.root_delay.to_double());
        fmt::println("root_dispersion     : {}", pay.root_dispersion.to_double());
        fmt::println("last_update_interval: {}", pay.last_update_interval.to_double());
        fmt::println("here");
    }
};

int main()
{
    boost::asio::io_context ioc{1};
    chrony_session sess(ioc, "/var/run/chrony/chronyd.sock");

    sess.start();
    ioc.run();
    printf("Done\n");
}