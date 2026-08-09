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
        CHRONY_BAD_VERSION,
        CHRONY_BAD_PACKET_TYPE,
        CHRONY_UNEXPECTED_COMMAND,
        CHRONY_UNEXPECTED_FORMAT,
        CHRONY_BAD_SEQUENCE_NUMBER,
        CHRONY_BAD_REPLY_STATUS
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
        source_num  = 14,
        source_data = 15,
        tracking    = 33,
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
        source_num  = 2,
        source_data = 3,
        tracking    = 5
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

    enum class source_mode : std::uint16_t
    {
        client          = 0,
        peer            = 1,
        reference_clock = 2
    };

    std::string_view to_string(const source_mode mode);

//----------------------------------------------------------------------------------------------------------------

    enum class source_state : std::uint16_t
    {
        selected      = 0, // *
        nonselectable = 1, // ?
        falseticker   = 2, // x
        jittery       = 3, // ~
        unselected    = 4, // +
        selectable    = 5  // -
    };

    std::string_view to_string(const source_state state);
    char state_symbol(const source_state state);

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

    struct request_source_data
    {
        int32_t index{};
    };

    static_assert(sizeof(request_source_data) == 4);

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

    struct payload_sources_num
    {
        uint32_t count{};
    };

    static_assert(sizeof(payload_sources_num) == 4);

//----------------------------------------------------------------------------------------------------------------

    struct payload_source_data
    {
        chrony_address  address{};
        int16_t         poll_base2{};
        uint16_t        stratum{};
        source_state    state{};
        source_mode     mode{};
        uint16_t        flags{};
        uint16_t        reachability{};
        uint32_t        since_sample{};
        chrony_float    original_measurement{};
        chrony_float    adjusted_measurement{};
        chrony_float    measurement_error{};
        std::chrono::microseconds poll() const;
    };

    static_assert(sizeof(payload_source_data) == 48);

//----------------------------------------------------------------------------------------------------------------

    void byteswap(chrony_float& flt);
    void byteswap(chrony_timestamp& ts);
    void byteswap(chrony_address& addr);
    void byteswap(request_header& hdr);
    void byteswap(request_source_data& pay);
    void byteswap(response_header& hdr);
    void byteswap(payload_tracking& pay);
    void byteswap(payload_sources_num& pay);
    void byteswap(payload_source_data& pay);

//----------------------------------------------------------------------------------------------------------------

    template<class ExecutionContext, class Executor>
    concept compatible_execution_context =
        std::is_convertible_v<ExecutionContext&, boost::asio::execution_context&> &&
        std::constructible_from<Executor, decltype(std::declval<ExecutionContext&>().get_executor())>;
   
//----------------------------------------------------------------------------------------------------------------

    template<class Executor>
    class basic_chrony_client
    {
    public:
        using executor_type = Executor;
        using udp           = boost::asio::ip::udp;
        using udp_socket    = boost::asio::basic_datagram_socket<udp, Executor>;
        template <typename Executor1> struct rebind_executor { using other = basic_chrony_client<Executor1>;};

    private:
        udp_socket          sock;
        std::mt19937        rand;
        std::vector<char>   bufwrite;
        std::vector<char>   bufread;

        struct async_read_tracking_impl;
        struct async_read_sources_impl;

    public:

        basic_chrony_client(const executor_type& ex);

        template <compatible_execution_context<executor_type> ExecutionContext>
        explicit basic_chrony_client(ExecutionContext& context);

        const auto& next_layer()          const noexcept;
        auto&       next_layer()                noexcept;
        auto&       lowest_layer()              noexcept;
        auto        get_executor()              noexcept;
        auto        get_cancellation_state()    noexcept;
        auto        get_allocator()       const noexcept;

        template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, payload_tracking)) CompletionToken = boost::asio::default_completion_token_t<Executor>>
        auto async_read_tracking (
            CompletionToken&& token = boost::asio::default_completion_token_t<Executor>()
        );

        template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, std::vector<payload_source_data>)) CompletionToken = boost::asio::default_completion_token_t<Executor>>
        auto async_read_sources (
            CompletionToken&& token = boost::asio::default_completion_token_t<Executor>()
        );
    };
    
    using chrony_client = basic_chrony_client<boost::asio::any_io_executor>;

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
// DEFINITIONS
//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

    template<class Executor>
    inline basic_chrony_client<Executor>::basic_chrony_client(const executor_type& ex)
    : sock(ex),
      rand(std::random_device{}())
    {
        // This ensures kernel filters received packets so only datagrams from the connected peer are delivered.
        sock.connect({boost::asio::ip::make_address_v4("127.0.0.1"), 323});
    }

    template<class Executor>
    template <compatible_execution_context<Executor> ExecutionContext>
    inline basic_chrony_client<Executor>::basic_chrony_client(ExecutionContext& context)                      
    : basic_chrony_client(executor_type(context.get_executor()))
    {
    }
  
//----------------------------------------------------------------------------------------------------------------

    template<class Executor> inline const auto& basic_chrony_client<Executor>::next_layer()       const noexcept {return sock;}
    template<class Executor> inline auto&       basic_chrony_client<Executor>::next_layer()             noexcept {return sock;}
    template<class Executor> inline auto&       basic_chrony_client<Executor>::lowest_layer()           noexcept {return sock.lowest_layer();}
    template<class Executor> inline auto        basic_chrony_client<Executor>::get_executor()           noexcept {return sock.get_executor();}
    template<class Executor> inline auto        basic_chrony_client<Executor>::get_cancellation_state() noexcept {return boost::asio::get_associated_cancellation_slot(sock);}
    template<class Executor> inline auto        basic_chrony_client<Executor>::get_allocator()    const noexcept {return boost::asio::get_associated_allocator(sock);}

//----------------------------------------------------------------------------------------------------------------

    template<class Enum>
    constexpr auto to_underlying(Enum value) noexcept
    {
        return static_cast<std::underlying_type_t<Enum>>(value);
    }

//----------------------------------------------------------------------------------------------------------------

    inline std::error_code parse_response_header (
        response_header&        hdr, 
        const request_command   cmd, 
        const reply_format      fmt, 
        const uint32_t          seq, 
        std::span<const char>   buf, 
        size_t&                 consumed
    )
    {
        if (buf.size() < sizeof(hdr))
            return make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA);

        memcpy(&hdr, &buf[0], sizeof(hdr)); 
        byteswap(hdr);
        consumed += sizeof(hdr);

        if      (hdr.version  != 6)
            return make_error_code(CHRONY_BAD_VERSION);
        else if (hdr.type     != to_underlying(packet_type::response))
            return make_error_code(CHRONY_BAD_PACKET_TYPE);
        else if (hdr.command  != to_underlying(cmd))
            return make_error_code(CHRONY_UNEXPECTED_COMMAND);
        else if (hdr.format   != to_underlying(fmt))
            return make_error_code(CHRONY_UNEXPECTED_FORMAT);
        else if (hdr.sequence != seq)
            return make_error_code(CHRONY_BAD_SEQUENCE_NUMBER);
        else if (hdr.status   != 0)
            return make_error_code(CHRONY_BAD_REPLY_STATUS);

        return {};
    }

    template<class Executor>
    struct basic_chrony_client<Executor>::async_read_tracking_impl
    {
        basic_chrony_client<Executor>&      client;
        uint32_t                            seq{};
        enum {writing, reading, parsing}    state{writing};

        async_read_tracking_impl(basic_chrony_client<Executor>& client_)
        : client{client_}
        {
        }

        template<class Self>
        void send_tracking_req(Self& self)
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
            client.sock.async_send(boost::asio::buffer(client.bufwrite), std::move(self));
        }

        template<class Self>
        void operator()(Self& self, boost::system::error_code error = {}, std::size_t ntransferred = 0)
        {
            // IO error
            if (error)
                self.complete(error, {});

            else if (state == writing)
            {
                state = reading;
                send_tracking_req(self);
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
                std::span<const char> buf(&client.bufread[0], ntransferred);

                // Deserialize header
                response_header  hdr{};
                payload_tracking pay{};
                size_t off{};

                auto ec = parse_response_header(hdr, request_command::tracking, reply_format::tracking, seq, buf, off);

                if (ec)
                {
                    self.complete(ec, {});
                    return;
                }

                // Deserialize payload
                if (buf.size() < (sizeof(hdr)+sizeof(pay)))
                {
                    self.complete(make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA), {});
                    return;
                }
                
                memcpy(&pay, &buf[off], sizeof(pay));
                byteswap(pay);
                self.complete({}, pay);
            }
        }
    };

    template<class Executor>
    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, payload_tracking)) CompletionToken>
    inline auto basic_chrony_client<Executor>::async_read_tracking (
        CompletionToken&& token
    )
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, payload_tracking)> (
            async_read_tracking_impl{*this},
            token, *this
        );
    }

//----------------------------------------------------------------------------------------------------------------

    template<class Executor>
    struct basic_chrony_client<Executor>::async_read_sources_impl
    {
        enum class state_t {writing_num, reading, parsing_num, parsing_data};
        basic_chrony_client<Executor>&      client;
        uint32_t                            seq{};
        std::vector<payload_source_data>    sources;
        int32_t                             count{};
        state_t                             state{state_t::writing_num};
        state_t                             next{};

        async_read_sources_impl(basic_chrony_client<Executor>& client_)
        : client{client_}
        {
        }

        template<class Self>
        void send_source_count_req(Self& self)
        {
            // Prepare request
            request_header req{};
            req.version     = 6;
            req.type        = to_underlying(packet_type::request);
            req.command     = to_underlying(request_command::source_num);
            req.sequence    = std::uniform_int_distribution<uint32_t>{}(client.rand);
            req.attempt     = 0;
            seq             = req.sequence;
            byteswap(req);

            // Resize buffer and serialize
            client.bufread.resize(sizeof(response_header) + sizeof(payload_sources_num));
            client.bufwrite.assign(client.bufread.size(), '\0');
            memcpy(&client.bufwrite[0], &req, sizeof(req));

            // Send
            client.sock.async_send(boost::asio::buffer(client.bufwrite), std::move(self));
        }

        template<class Self>
        void send_source_data_req(Self& self)
        {
            // Prepare request
            request_header      req{};
            request_source_data pay{};
            req.version     = 6;
            req.type        = to_underlying(packet_type::request);
            req.command     = to_underlying(request_command::source_data);
            req.sequence    = std::uniform_int_distribution<uint32_t>{}(client.rand);
            req.attempt     = 0;
            seq             = req.sequence;
            pay.index       = count;
            byteswap(req);
            byteswap(pay);

            // Resize buffer and serialize
            client.bufread.resize(sizeof(response_header) + sizeof(payload_source_data));
            client.bufwrite.assign(client.bufread.size(), '\0');
            size_t off{};
            memcpy(&client.bufwrite[off], &req, sizeof(req)); off += sizeof(req);
            memcpy(&client.bufwrite[off], &pay, sizeof(pay)); off += sizeof(pay);

            // Send
            client.sock.async_send(boost::asio::buffer(client.bufwrite), std::move(self));
        }
        
        template<class Self>
        void operator()(Self& self, boost::system::error_code error = {}, std::size_t ntransferred = 0)
        {
            // IO error
            if (error)
                self.complete(error, {});

            else if (state == state_t::writing_num)
            {
                state = state_t::reading;
                next  = state_t::parsing_num;
                send_source_count_req(self);
            }

            else if (state == state_t::reading)
            {
                if (ntransferred != client.bufwrite.size())
                    self.complete(make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA), {});

                else
                {
                    // Read
                    state = next;
                    client.sock.async_receive(boost::asio::buffer(client.bufread), std::move(self));
                }
            }

            else if (state == state_t::parsing_num)
            {
                std::span<const char> buf(&client.bufread[0], ntransferred);

                // Deserialize header
                response_header     hdr{};
                payload_sources_num pay{};
                size_t off{};

                auto ec = parse_response_header(hdr, request_command::source_num, reply_format::source_num, seq, buf, off);

                if (ec)
                {
                    self.complete(ec, {});
                    return;
                }

                // Deserialize payload
                if (buf.size() < (sizeof(hdr)+sizeof(pay)))
                {
                    self.complete(make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA), {});
                    return;
                }
                
                memcpy(&pay, &buf[off], sizeof(pay));
                byteswap(pay);

                if (pay.count > 0)
                {
                    sources.resize(pay.count);
                    state = state_t::reading;
                    next  = state_t::parsing_data;
                    send_source_data_req(self);
                }
                else
                {
                    self.complete({}, std::move(sources));
                }
            }
            else if (state == state_t::parsing_data)
            {
                std::span<const char> buf(&client.bufread[0], ntransferred);

                // Deserialize header
                response_header     hdr{};
                payload_source_data pay{};
                size_t off{};

                auto ec = parse_response_header(hdr, request_command::source_data, reply_format::source_data, seq, buf, off);

                if (ec)
                {
                    self.complete(ec, {});
                    return;
                }

                // Deserialize payload
                if (buf.size() < (sizeof(hdr)+sizeof(pay)))
                {
                    self.complete(make_error_code(CHRONY_TRANSACTION_INSUFFICIENT_DATA), {});
                    return;
                }
                
                memcpy(&pay, &buf[off], sizeof(pay));
                byteswap(pay);

                sources[count++] = std::move(pay);

                if (count < sources.size())
                {
                    state = state_t::reading;
                    send_source_data_req(self);
                }
                else
                {
                    self.complete({}, std::move(sources));
                }
            }
        }
    };

    template<class Executor>
    template<BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code, std::vector<payload_source_data>)) CompletionToken>
    inline auto basic_chrony_client<Executor>::async_read_sources (
        CompletionToken&& token
    )
    {
        return boost::asio::async_compose<CompletionToken, void(boost::system::error_code, std::vector<payload_source_data>)> (
            async_read_sources_impl{*this},
            token, *this
        );
    }

//----------------------------------------------------------------------------------------------------------------

}