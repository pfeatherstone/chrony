#include <cmath>
#include "chrony.h"

using namespace std::chrono;

namespace chrony
{

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

    std::string_view to_string(const leap_status status)
    {
        switch(status)
        {
            case normal             : return "Normal";
            case insert_second      : return "Insert second";
            case delete_second      : return "Delete second";
            case not_synchronized   : return "Not synchronized";
            default                 : return "Unknown";
        }
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
        // pay.address
        pay.stratum         = ntohs(pay.stratum);
        pay.status          = static_cast<leap_status>(ntohs(static_cast<uint16_t>(pay.status)));
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

//----------------------------------------------------------------------------------------------------------------

}