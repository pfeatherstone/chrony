#include <random>
#include <functional>
#include <boost/asio/io_context.hpp>
#include <fmt/base.h>
#include <fmt/chrono.h>
#include "chrony.h"

using namespace chrony;

enum chrony_error
{

};

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
        request_header hdr{};
        hdr.version     = 6;
        hdr.type        = 1;    // request
        hdr.command     = tracking;
        hdr.sequence    = std::uniform_int_distribution<uint16_t>{}(rand);
        hdr.attempt     = 0;
        hdr.byteswap();
        bufwrite.resize(sizeof(response_header) + sizeof(payload_tracking));
        bufread.resize(bufwrite.size());
        memcpy(&bufwrite[0], &hdr, sizeof(hdr));
        sock.async_send_to(boost::asio::buffer(bufwrite), remote_endpoint, std::bind_front(&chrony_session::on_send, this));
    }

    void on_send(boost::system::error_code ec, size_t nwritten)
    {
        if (ec)
        {
            printf("on_send() error : %s\n", ec.message().c_str());
            return;
        }

        sock.async_receive_from(boost::asio::buffer(bufread), remote_endpoint, std::bind_front(&chrony_session::on_read, this));
    }

    void on_read(boost::system::error_code ec, size_t nread)
    {
        if (ec)
        {
            printf("on_read() error : %s\n", ec.message().c_str());
            return;
        }

        if (nread != sizeof(response_header) + sizeof(payload_tracking))
        {
            printf("bad reply length\n");
            return;
        }

        response_header  hdr{};
        payload_tracking pay{};
        size_t off{};
        memcpy(&hdr, &bufread[off], sizeof(hdr)); off += sizeof(hdr);
        memcpy(&pay, &bufread[off], sizeof(pay)); off += sizeof(pay);
        hdr.byteswap();
        pay.byteswap();
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