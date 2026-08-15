#include "doctest.h"
#include <boost/asio/io_context.hpp>
#include <chrony.h>

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