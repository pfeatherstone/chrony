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
        CHRONY_TRANSACTION_INSUFFICIENT_DATA = 1,
        CHRONY_BAD_PACKET_TYPE,
        CHRONY_UNEXPECTED_COMMAND,
        CHRONY_UNEXPECTED_FORMAT,
        CHRONY_BAD_SEQUENCE_NUMBER
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

    enum class packet_type : uint8_t
    {
        request  = 1,
        response = 2
    };

//----------------------------------------------------------------------------------------------------------------

    enum class request_command : uint16_t
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

    enum class reply_format : uint16_t
    {
        tracking = 5
    };

//----------------------------------------------------------------------------------------------------------------

    enum class leap_status : uint16_t
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

    void byteswap(chrony_float& flt);
    void byteswap(chrony_timestamp& ts);
    void byteswap(chrony_address& addr);
    void byteswap(request_header& hdr);
    void byteswap(response_header& hdr);
    void byteswap(payload_tracking& pay);

//----------------------------------------------------------------------------------------------------------------

    template<class Executor>
    class chrony_client
    {
    public:
        using executor_type = Executor;
        using udp           = boost::asio::ip::udp;
        using udp_socket    = boost::asio::basic_datagram_socket<udp, Executor>;
        template <typename Executor1> struct rebind_executor { using other = chrony_client<Executor1>;};

    private:
        udp_socket          sock;
        std::mt19937        rand;
        std::vector<char>   bufwrite;
        std::vector<char>   bufread;

        struct async_read_tracking_impl;

    public:
        const auto& next_layer()                const noexcept {return sock;}
        auto&       next_layer()                      noexcept {return sock;}
        auto&       lowest_layer()                    noexcept {return sock.lowest_layer();}
        auto        get_executor()                    noexcept {return sock.get_executor();}
        auto        get_cancellation_state()          noexcept {return boost::asio::get_associated_cancellation_slot(sock);}
        auto        get_allocator()             const noexcept {return boost::asio::get_associated_allocator(sock);}

        chrony_client(const executor_type& ex)
        : sock(ex),
          rand(std::random_device{}())
        {
            // This ensures kernel filters received packets so only datagrams from the connected peer are delivered.
            sock.connect({boost::asio::ip::make_address_v4("127.0.0.1"), 323});
        }

        template <typename ExecutionContext>
            requires (
                std::is_convertible_v<ExecutionContext&, boost::asio::execution_context&> &&
                std::constructible_from<executor_type, decltype(std::declval<ExecutionContext&>().get_executor())>
            )
        explicit chrony_client(ExecutionContext& context)                      
        : chrony_client(executor_type(context.get_executor()))
        {
        }

        template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, payload_tracking)) CompletionToken = boost::asio::default_completion_token_t<Executor>>
        auto async_read_tracking (
            CompletionToken&& token = boost::asio::default_completion_token_t<Executor>()
        );
    };
    
//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
// DEFINITIONS
//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

    namespace details
    {
        template<class Enum>
        constexpr auto to_underlying(Enum value) noexcept
        {
            return static_cast<std::underlying_type_t<Enum>>(value);
        }
    }

//----------------------------------------------------------------------------------------------------------------

    template<class Executor>
    struct chrony_client<Executor>::async_read_tracking_impl
    {
        chrony_client<Executor>&            client;
        uint32_t                            seq{};
        enum {writing, reading, parsing}    state{writing};

        async_read_tracking_impl(chrony_client<Executor>& client_)
        : client{client_}
        {
        }
        
        template<class Self>
        void operator()(Self& self, boost::system::error_code error = {}, std::size_t ntransferred = 0)
        {
            using details::to_underlying;

            // IO error
            if (error)
                self.complete(error, {});

            else if (state == writing)
            {
                // Prepare request
                request_header req{};
                req.version     = 6;
                req.type        = to_underlying(packet_type::request);
                req.command     = to_underlying(request_command::tracking);
                req.sequence    = std::uniform_int_distribution<uint32_t>{}(client.rand);
                req.attempt     = 0;
                seq             = req.sequence;
                byteswap(req);

                // Resize buffer and serialize
                client.bufread.resize(sizeof(response_header) + sizeof(payload_tracking));
                client.bufwrite.resize(client.bufread.size());
                memcpy(&client.bufwrite[0], &req, sizeof(req));

                // Send
                state = reading;
                client.sock.async_send(boost::asio::buffer(client.bufwrite), std::move(self));
            }

            else if (state == reading)
            {
                if (ntransferred != client.bufwrite.size())
                    self.complete(make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA), {});

                else
                {
                    // Read
                    state = parsing;
                    client.sock.async_receive(boost::asio::buffer(client.bufread), std::move(self));
                }
            }

            else if (state == parsing)
            {
                if (ntransferred != client.bufread.size())
                    self.complete(make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA), {});
                
                else
                {
                    // Deserialise
                    response_header  hdr{};
                    payload_tracking pay{};
                    size_t off{};
                    memcpy(&hdr, &client.bufread[off], sizeof(hdr)); off += sizeof(hdr);
                    memcpy(&pay, &client.bufread[off], sizeof(pay)); off += sizeof(pay);
                    byteswap(hdr);
                    byteswap(pay);

                    if      (hdr.type     != to_underlying(packet_type::response))
                        self.complete(make_error_code(CHRONY_BAD_PACKET_TYPE), {});
                    else if (hdr.command  != to_underlying(request_command::tracking))
                        self.complete(make_error_code(CHRONY_UNEXPECTED_COMMAND), {});
                    else if (hdr.format   != to_underlying(reply_format::tracking))
                        self.complete(make_error_code(CHRONY_UNEXPECTED_FORMAT), {});
                    else if (hdr.sequence != seq)
                        self.complete(make_error_code(CHRONY_BAD_SEQUENCE_NUMBER), {});
                    else
                        self.complete({}, pay);
                }
            }
        }
    };

//----------------------------------------------------------------------------------------------------------------

    template<class Executor>
    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, payload_tracking)) CompletionToken>
    inline auto chrony_client<Executor>::async_read_tracking(
        CompletionToken&& token
    )
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, payload_tracking)> (
            async_read_tracking_impl{*this},
            token, *this
        );
    }

//----------------------------------------------------------------------------------------------------------------

}