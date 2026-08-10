#pragma once

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/base.h>
#include "chrony.h"

inline std::string format_address(const chrony::chrony_address& address)
{
    if (const auto ip = address.to_address()) return ip->to_string();
    if (const auto id = address.to_id())      return fmt::format("ID:{:08X}", *id);
    else                                      return "<unspecified>";
}

inline void print(const chrony::payload_tracking& pay)
{
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

inline void print(const std::vector<chrony::payload_source_data>& sources)
{
    using std::chrono::duration_cast;
    using std::chrono::seconds;
    
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