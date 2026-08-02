#pragma once

#include <cstdint>
#include <chrono>
#include <random>
#include <string_view>
#include <boost/asio/ip/udp.hpp>

namespace chrony
{

//----------------------------------------------------------------------------------------------------------------

    struct chrony_float
    {
        uint32_t data{};
        double to_double() const;
    };

//----------------------------------------------------------------------------------------------------------------

    struct chrony_timestamp
    {
        uint32_t sec_high{};
        uint32_t sec_low{};
        uint32_t nano{};
        auto to_time_point() const -> std::chrono::system_clock::time_point;
    };

//----------------------------------------------------------------------------------------------------------------

    enum request_command : uint16_t
    {
        tracking    = 33,
        sources     = 14,
        source_data = 15,
        sourcestats = 34,
        rtcdata     = 35,
        activity    = 44,
        smoothing   = 51,
        serverstats = 54,
        ntpdata     = 57,
        authdata    = 67,
        selectdata  = 69
    };

//----------------------------------------------------------------------------------------------------------------

    enum leap_status : uint16_t
    {
        normal = 0,
        insert_second,
        delete_second,
        not_synchronized
    };

    std::string_view to_string(const leap_status status);

//----------------------------------------------------------------------------------------------------------------

    struct request_header
    {
        uint8_t  version{};
        uint8_t  type{};
        uint8_t  pad0[2];
        uint16_t command{};
        uint16_t attempt{};
        uint32_t sequence{};
        uint8_t  pad[8];
    };

    static_assert(sizeof(request_header) == 20);

//----------------------------------------------------------------------------------------------------------------

    struct response_header
    {
        uint8_t  version{};
        uint8_t  type{};
        uint8_t  pad0[2];
        uint16_t command{};
        uint16_t format{};
        uint16_t status{};
        uint8_t  pad1[6];
        uint32_t sequence{};
        uint8_t  pad2[8];
    };

    static_assert(sizeof(response_header) == 28);

//----------------------------------------------------------------------------------------------------------------

    struct payload_tracking
    {
        uint32_t            reference_id{};
        uint8_t             address[20];
        uint16_t            stratum{};
        leap_status         status{};
        chrony_timestamp    ref_time{};
        chrony_float        current_correction{};
        chrony_float        last_offset{};
        chrony_float        rms_offset{};
        chrony_float        freq_offset_ppm{};
        chrony_float        freq_residual_ppm{};
        chrony_float        skew_ppm{};
        chrony_float        root_delay{};
        chrony_float        root_dispersion{};
        chrony_float        last_update_interval{};
    };

    static_assert(sizeof(payload_tracking) == 76);

//----------------------------------------------------------------------------------------------------------------

    template<class Executor>
    class chrony_client
    {
    public:
        using executor_type = Executor;
        template <typename Executor1> struct rebind_executor { using other = chrony_client<Executor1>;};

    private:
        using udp           = boost::asio::ip::udp;
        using udp_socket    = boost::asio::basic_datagram_socket<udp, Executor>;

        udp_socket          sock;
        udp::endpoint       remote_endpoint;
        std::mt19937        rand;
        std::vector<char>   bufwrite;
        std::vector<char>   bufread;

    public:

        chrony_client(const executor_type& ex)
        : sock(ex, udp::endpoint(udp::v4(), 0)),
          remote_endpoint(boost::asio::ip::make_address_v4("127.0.0.1"), 323),
          rand(time(NULL))
        {
        }

        // template <typename ExecutionContext>
        // chrony_client(ExecutionContext& context) requires std::is_convertible_v<ExecutionContext&, boost::asio::execution_context&>
        // : chrony_client(executor_type(context.get_executor()))
        // {
        // }
    };

//----------------------------------------------------------------------------------------------------------------

}