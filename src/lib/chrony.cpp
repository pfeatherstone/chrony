#include <cmath>
#include "chrony.h"

using namespace std::chrono;

namespace chrony
{

//----------------------------------------------------------------------------------------------------------------

    struct chrony_error_category : std::error_category
    {
        const char* name() const noexcept override 
        {
            return "chrony_error_category";
        }

        std::string message(int ev) const override
        {
            switch(static_cast<chrony_error>(ev))
            {
            case CHRONY_SERVICE_NOT_AVAILABLE:              return "Chronyd service not available";
            case CHRONY_TRANSACTION_INSUFFICIENT_DATA:      return "Insufficient data while writing/reading request/response";
            case CHRONY_BAD_VERSION:                        return "Bad version (!6)";
            case CHRONY_BAD_PACKET_TYPE:                    return "Bad packet type.";
            case CHRONY_UNEXPECTED_COMMAND:                 return "Received unexpected command type (e.g. not matching request)";
            case CHRONY_UNEXPECTED_FORMAT:                  return "Received unexpected reply format type.";
            case CHRONY_BAD_SEQUENCE_NUMBER:                return "Received non-matching sequence number in response";
            case CHRONY_BAD_REPLY_STATUS:                   return "Received bad status in reply";
            default:                                        return "Unrecognised error";
            }
        }
    };

    const chrony_error_category chrony_error_category_;

    std::error_code make_error_code(chrony_error ec)
    {
        return {static_cast<int>(ec), chrony_error_category_};
    }

//----------------------------------------------------------------------------------------------------------------

    double chrony_float::to_double() const
    {
        constexpr uint32_t mask = (1u << 25) - 1;
        int32_t exp   = data >> 25;
        if (exp >= (1 << 6)) exp -= (1 << 7);
        int32_t coeff = data & mask;
        if (coeff >= (1 << 24)) coeff -= (1 << 25);
        return std::ldexp((double)coeff, exp-25);
    }
    
//----------------------------------------------------------------------------------------------------------------

    auto chrony_timestamp::to_time_point() const -> system_clock::time_point
    {
        const uint64_t sec = (static_cast<uint64_t>(sec_high) << 32) | static_cast<uint64_t>(sec_low);
        return system_clock::time_point{seconds{sec} + nanoseconds{nano}};
    }
     
//----------------------------------------------------------------------------------------------------------------

    bool chrony_address::is_ipv4()    const { return family == 1; }
    bool chrony_address::is_ipv6()    const { return family == 2; }
    bool chrony_address::is_id()      const { return family == 3; }
    bool chrony_address::is_ip()      const { return is_ipv4() || is_ipv6(); }

    auto chrony_address::to_address() const -> std::optional<boost::asio::ip::address>
    {
        // IPv4
        if (is_ipv4())
        {
            boost::asio::ip::address_v4::bytes_type bytes{};
            memcpy(&bytes[0], addr, bytes.size());
            return boost::asio::ip::address_v4{bytes};
        }
        // IPv6
        else if (is_ipv6())
        {
            boost::asio::ip::address_v6::bytes_type bytes{};
            memcpy(&bytes[0], addr, bytes.size());
            return boost::asio::ip::address_v6{bytes};
        }
        else
        {
            return std::nullopt;
        }
    }

    auto chrony_address::to_id() const -> std::optional<uint32_t>
    {
        // ID
        if (is_id())
        {
            uint32_t network_id{};
            memcpy(&network_id, &addr, 4);
            return network_id;
        }
        else
        {
            return std::nullopt;
        }
    }

//----------------------------------------------------------------------------------------------------------------

    std::string_view to_string(const leap_status status)
    {
        switch(status)
        {
            case leap_status::normal            : return "Normal";
            case leap_status::insert_second     : return "Insert second";
            case leap_status::delete_second     : return "Delete second";
            case leap_status::not_synchronized  : return "Not synchronized";
            default                             : return "Unknown";
        }
    }

    std::string_view to_string(const source_mode mode)
    {
        switch (mode)
        {
        case source_mode::client            : return "server";
        case source_mode::peer              : return "peer";
        case source_mode::reference_clock   : return "refclock";
        default                             : return "unknown";
        }
    }

    std::string_view to_string(const source_state state)
    {
        switch (state)
        {
            case source_state::selected         : return "selected";
            case source_state::nonselectable    : return "unusable";
            case source_state::falseticker      : return "falseticker";
            case source_state::jittery          : return "jittery";
            case source_state::unselected       : return "combined";
            case source_state::selectable       : return "selectable";
            default                             : return "unknown";
        }
    }

    char state_symbol(const source_state state)
    {
        switch (state)
        {
            case source_state::selected:      return '*';
            case source_state::nonselectable: return '?';
            case source_state::falseticker:   return 'x';
            case source_state::jittery:       return '~';
            case source_state::unselected:    return '+';
            case source_state::selectable:    return '-';
            default:                          return '?';
        }
    }

//----------------------------------------------------------------------------------------------------------------

    microseconds payload_source_data::poll() const
    {
        return microseconds(static_cast<int64_t>(std::ldexp(1e6, poll_base2)));
    }

//----------------------------------------------------------------------------------------------------------------

    void byteswap(chrony_float& flt)
    {
        flt.data = ntohl(flt.data);
    }

    void byteswap(chrony_timestamp& ts)
    {
        ts.sec_high = ntohl(ts.sec_high);
        ts.sec_low  = ntohl(ts.sec_low);
        ts.nano     = ntohl(ts.nano);
    }

    void byteswap(chrony_address& addr)
    {
        addr.family = ntohs(addr.family);

        if (addr.is_id())
        {
            uint32_t network_id{};
            memcpy(&network_id, &addr.addr, 4);
            network_id = ntohl(network_id);
            memcpy(&addr.addr, &network_id, 4);
        }
    }

    void byteswap(request_header& hdr)
    {
        hdr.command  = ntohs(hdr.command);
        hdr.attempt  = ntohs(hdr.attempt);
        hdr.sequence = ntohl(hdr.sequence);
    }

    void byteswap(response_header& hdr)
    {
        hdr.command     = ntohs(hdr.command);
        hdr.format      = ntohs(hdr.format);
        hdr.status      = ntohs(hdr.status);
        hdr.sequence    = ntohl(hdr.sequence);
    }

    void byteswap(payload_tracking& pay)
    {
        pay.reference_id    = ntohl(pay.reference_id);
        pay.stratum         = ntohs(pay.stratum);
        pay.status          = static_cast<leap_status>(ntohs(static_cast<uint16_t>(pay.status)));
        byteswap(pay.address);
        byteswap(pay.ref_time);
        byteswap(pay.current_correction);
        byteswap(pay.last_offset);
        byteswap(pay.rms_offset);
        byteswap(pay.freq_offset_ppm);
        byteswap(pay.freq_residual_ppm);
        byteswap(pay.skew_ppm);
        byteswap(pay.root_delay);
        byteswap(pay.root_dispersion);
        byteswap(pay.last_update_interval);
    }

    void byteswap(payload_sources_num& pay)
    {
        pay.count = ntohl(pay.count);
    }

    void byteswap(payload_source_data& pay)
    {
        byteswap(pay.address);
        pay.poll_base2      = static_cast<int16_t>(ntohs(static_cast<uint16_t>(pay.poll_base2)));
        pay.stratum         = ntohs(pay.stratum);
        pay.state           = static_cast<source_state>(ntohs(static_cast<std::uint16_t>(pay.state)));
        pay.mode            = static_cast<source_mode>(ntohs(static_cast<std::uint16_t>(pay.mode)));
        pay.flags           = ntohs(pay.flags);
        pay.reachability    = ntohs(pay.reachability);
        pay.since_sample    = ntohl(pay.since_sample);
        byteswap(pay.original_measurement);
        byteswap(pay.adjusted_measurement);
        byteswap(pay.measurement_error);
    }

    void byteswap(payload_sourcestats& pay)
    {
       pay.reference_id = ntohl(pay.reference_id);
       byteswap(pay.address);
       pay.n_samples    = ntohl(pay.n_samples);
       pay.n_runs       = ntohl(pay.n_runs);
       pay.span_seconds = ntohl(pay.span_seconds);
       byteswap(pay.sample_stdev);
       byteswap(pay.freq_residual_ppm);
       byteswap(pay.skew_ppm);   
       byteswap(pay.estimated_offset);
       byteswap(pay.estimated_offset_error);
    }

//----------------------------------------------------------------------------------------------------------------

}