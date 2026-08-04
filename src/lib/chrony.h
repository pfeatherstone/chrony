#pragma once

#include <cstdint>
#include <chrono>
#include <random>
#include <string_view>
#include <vector>
#include <optional>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/compose.hpp>

namespace chrony
{

//----------------------------------------------------------------------------------------------------------------

    enum chrony_error
    {
        CHRONY_TRANSACTION_INSUFFICIENT_DATA = 1
    };

    std::error_code make_error_code(chrony_error ec);

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

    struct chrony_address
    {
        uint8_t  addr[16];
        uint16_t family{};
        uint8_t  pad[2];
        bool is_ipv4()    const;
        bool is_ipv6()    const;
        bool is_ip()      const;
        bool is_id()      const;
        auto to_address() const -> std::optional<boost::asio::ip::address>;
        auto to_id()      const -> std::optional<uint32_t>;
    };

    static_assert(sizeof(chrony_address) == 20);

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
        uint8_t  pad1[8];
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
        chrony_address      address;
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

    void prepare_tracking_request (
        request_header&     req, 
        std::vector<char>&  buf,
        std::mt19937&       rng
    );

    void deserialize_tracking_response (
        response_header&            reply, 
        payload_tracking&           pay, 
        const std::vector<char>&    buf
    );

//----------------------------------------------------------------------------------------------------------------

    template<class Executor>
    struct chrony_client
    {
        using executor_type = Executor;
        using udp           = boost::asio::ip::udp;
        using udp_socket    = boost::asio::basic_datagram_socket<udp, Executor>;
        template <typename Executor1> struct rebind_executor { using other = chrony_client<Executor1>;};

        udp_socket          sock;
        udp::endpoint       remote_endpoint;
        std::mt19937        rand;
        std::vector<char>   bufwrite;
        std::vector<char>   bufread;

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

        const auto& next_layer()                const noexcept {return sock;}
        auto&       next_layer()                      noexcept {return sock;}
        auto&       lowest_layer()                    noexcept {return sock.lowest_layer();}
        auto        get_executor()                    noexcept {return sock.get_executor();}
        auto        get_cancellation_state()          noexcept {return boost::asio::get_associated_cancellation_slot(sock);}
        auto        get_allocator()             const noexcept {return boost::asio::get_associated_allocator(sock);}
    };

//----------------------------------------------------------------------------------------------------------------

    template <
      class Executor, 
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, const payload_tracking&)) CompletionToken = boost::asio::default_completion_token_t<Executor>
    >
    auto async_read_tracking(
        chrony_client<Executor>& sock,
        CompletionToken&&        token = boost::asio::default_completion_token_t<Executor>()
    );
    
//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
// DEFINITIONS
//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

    namespace details
    {
        template<class Executor>
        struct async_read_tracking_impl
        {
            chrony_client<Executor>& client;
            request_header           req{};
            enum {writing, reading, parsing} state{writing};

            async_read_tracking_impl(chrony_client<Executor>& client_)
            : client{client_}
            {
            }
            
            template<class Self>
            void operator()(Self& self, boost::system::error_code error = {}, std::size_t ntransferred = 0)
            {
                // IO error
                if (error)
                    self.complete(error, {});

                else if (state == writing)
                {
                    // Prepare request
                    prepare_tracking_request(req, client.bufwrite, client.rand);
                    client.bufread.resize(client.bufwrite.size());

                    // Send
                    state = reading;
                    client.sock.async_send_to(boost::asio::buffer(client.bufwrite), client.remote_endpoint, std::move(self));
                }

                else if (state == reading)
                {
                    if (ntransferred != client.bufwrite.size())
                        self.complete(make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA), {});

                    else
                    {
                        // Read
                        state = parsing;
                        client.sock.async_receive_from(boost::asio::buffer(client.bufread), client.remote_endpoint, std::move(self));
                    }
                }

                else if (state == parsing)
                {
                    if (ntransferred != client.bufwrite.size())
                        self.complete(make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA), {});
                    
                    else
                    {
                        response_header  hdr{};
                        payload_tracking pay{};
                        deserialize_tracking_response(hdr, pay, client.bufread);
                        self.complete({}, pay);
                    }
                }
            }
        };
    }

    template <
      class Executor, 
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, const payload_tracking&)) CompletionToken
    >
    inline auto async_read_tracking(
        chrony_client<Executor>& sock,
        CompletionToken&&        token
    )
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, const payload_tracking&)> (
            details::async_read_tracking_impl<Executor>{sock},
            token, sock
        );
    }

//----------------------------------------------------------------------------------------------------------------

}