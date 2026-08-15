# Chrony

A C++ chronyd client library using Asio [composed operations](https://think-async.com/Asio/asio-1.38.2/doc/asio/overview/composition/compose.html).

## Installation

Copy the contents of `src/lib` into your project then link to Boost::asio. 

## Examples

Try out:
- [client_callback.cpp](src/examples/client_callback.cpp)
- [client_awaitable.cpp](src/examples/client_awaitable.cpp)
- [client_coro.cpp](src/examples/client_coro.cpp)

Build using:

```bash
$ cmake ./examples -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build --parallel
```

Example code:

```cpp

awaitable<void> print_chrony_tracking(chrony_client& sock)
{
    try
    {
        const payload_tracking pay = co_await sock.async_read_tracking();

        fmt::println("Tracking results:");
        fmt::println("  reference_id        : {:08X}", pay.reference_id);
        fmt::println("  address             : {}", pay.address.to_address().value().to_string());
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
    catch(const std::exception& e)
    {
        fmt::println(stderr, "Error: {}", e.what());
    }
}

boost::asio::io_context ioc{1};
chrony_client sock(ioc);
co_spawn(ioc, print_chrony_tracking(sock), detached);
```