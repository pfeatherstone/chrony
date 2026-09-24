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
    fmt::println("  current_correction  : {}", to_double(pay.current_correction));
    fmt::println("  last_offset         : {}", to_double(pay.last_offset));
    fmt::println("  rms_offset          : {}", to_double(pay.rms_offset));
    fmt::println("  freq_offset_ppm     : {}", to_double(pay.freq_offset_ppm));
    fmt::println("  freq_residual_ppm   : {}", to_double(pay.freq_residual_ppm));
    fmt::println("  skew_ppm            : {}", to_double(pay.skew_ppm));
    fmt::println("  root_delay          : {}", to_double(pay.root_delay));
    fmt::println("  root_dispersion     : {}", to_double(pay.root_dispersion));
    fmt::println("  last_update_interval: {}", to_double(pay.last_update_interval));
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
            static_cast<int64_t>(to_double(source.adjusted_measurement)*1e6),
            static_cast<int64_t>(to_double(source.original_measurement)*1e6),
            static_cast<int64_t>(to_double(source.measurement_error)*1e6)
        );
    }
    fmt::println("");
}

inline void print(const std::vector<chrony::payload_sourcestats>& stats)
{
    fmt::println("Chrony source stats: {}\n", stats.size());

    fmt::println("{:<10} {:<20} {:>7} {:>7} {:>9} {:>12} {:>12} {:>10} {:>12} {:>12}",
        "RefID", "Address", "Samples", "Runs", "Span(s)", "StdDev(us)", "Resid(ppm)", "Skew(ppm)", "Offset(us)", "OffsetErr(us)");

    for (const auto& stat : stats)
    {
        fmt::println("{:08X}   {:<20} {:>7} {:>7} {:>9} {:>12.3f} {:+12.6f} {:>10.6f} {:+12.3f} {:>12.3f}",
            stat.reference_id,
            format_address(stat.address),
            stat.n_samples,
            stat.n_runs,
            stat.span_seconds,
            to_double(stat.sample_stdev) * 1e6,
            to_double(stat.freq_residual_ppm),
            to_double(stat.skew_ppm),
            to_double(stat.estimated_offset) * 1e6,
            to_double(stat.estimated_offset_error) * 1e6
        );
    }
    fmt::println("");
}
