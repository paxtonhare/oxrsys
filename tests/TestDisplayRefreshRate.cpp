// SPDX-License-Identifier: MPL-2.0

#include <catch2/catch_test_macros.hpp>

#include "DisplayRefreshRate.h"

using oxr::display_refresh::SelectAdvertisedRefreshRate;

TEST_CASE("Accepted asynchronous display refresh requests advertise the requested rate",
          "[quest][refresh]")
{
    REQUIRE(SelectAdvertisedRefreshRate(
                72.0f,
                true,
                90.0f,
                90) == 72);
}

TEST_CASE("Without an accepted request the active display refresh rate is advertised",
          "[quest][refresh]")
{
    REQUIRE(SelectAdvertisedRefreshRate(
                72.0f,
                false,
                90.0f,
                72) == 90);
    REQUIRE(SelectAdvertisedRefreshRate(
                72.0f,
                false,
                0.0f,
                80) == 80);
}
