// SPDX-License-Identifier: MPL-2.0

#include <catch2/catch_test_macros.hpp>

#include "FramePacer.h"

#include <chrono>

TEST_CASE("FramePacer keeps an absolute cadence after a late wake", "[frame][pacing]")
{
    using namespace std::chrono_literals;
    using Clock = FramePacer::Clock;

    FramePacer pacer;
    const Clock::time_point start{};

    CHECK(pacer.NextDeadline(start, 10ms) == start + 10ms);

    // Waking 2 ms late must not move every later deadline 2 ms later.
    CHECK(pacer.NextDeadline(start + 12ms, 10ms) == start + 20ms);
    CHECK(pacer.NextDeadline(start + 23ms, 10ms) == start + 30ms);
}

TEST_CASE("FramePacer resets cleanly when the negotiated refresh period changes",
          "[frame][pacing]")
{
    using namespace std::chrono_literals;
    using Clock = FramePacer::Clock;

    FramePacer pacer;
    const Clock::time_point start{};

    CHECK(pacer.NextDeadline(start, 10ms) == start + 10ms);
    CHECK(pacer.NextDeadline(start + 4ms, 20ms) == start + 24ms);
}

TEST_CASE("FramePacer skips missed frames and resets after suspension", "[frame][pacing]")
{
    using namespace std::chrono_literals;
    const FramePacer::Clock::time_point start{};
    FramePacer pacer;
    CHECK(pacer.NextDeadline(start, 10ms) == start + 10ms);
    CHECK(pacer.NextDeadline(start + 35ms, 10ms) == start + 40ms);
    CHECK(pacer.NextDeadline(start + 1s, 10ms) == start + 1010ms);
    pacer.Reset();
    CHECK(pacer.NextDeadline(start, 10ms) == start + 10ms);
}
