#include "doctest.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/as_tuple.hpp>
#include <chrony.h>

using boost::asio::detached;
using boost::asio::spawn;
using boost::asio::co_spawn;
using boost::asio::as_tuple;
using boost::asio::use_awaitable;
using boost::asio::awaitable;
using boost::asio::yield_context;

TEST_CASE("tracking [callback]")
{
    boost::asio::io_context ioc;
    chrony::chrony_client client{ioc};

    bool tracking_done{};

    client.async_read_tracking([&](boost::system::error_code ec, chrony::payload_tracking) {
        CHECK(!ec);
        tracking_done = true;
    });

    ioc.run();
    CHECK(tracking_done);
}

TEST_CASE("tracking [coro]")
{
    boost::asio::io_context ioc;
    chrony::chrony_client client{ioc};
    bool tracking_done{};

    const auto fn = [&](yield_context yield)
    {
        boost::system::error_code ec{};
        const auto pay = client.async_read_tracking(yield[ec]);
        CHECK(!ec);
        tracking_done = true;
    };

    spawn(ioc, fn, detached);
    ioc.run();
    CHECK(tracking_done);
}

TEST_CASE("tracking [awaitable]")
{
    boost::asio::io_context ioc;
    chrony::chrony_client client{ioc};
    bool tracking_done{};

    const auto fn = [&]() -> awaitable<void>
    {
        const auto [ec, pay] = co_await client.async_read_tracking(as_tuple(use_awaitable));
        CHECK(!ec);
        tracking_done = true;
    };

    co_spawn(ioc, fn, detached);
    ioc.run();
    CHECK(tracking_done);
}