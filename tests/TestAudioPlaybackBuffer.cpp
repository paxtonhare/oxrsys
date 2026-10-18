// SPDX-License-Identifier: MPL-2.0

#include <catch2/catch_test_macros.hpp>

#include "AudioPlaybackBuffer.h"

#include <array>

TEST_CASE("AudioPlaybackBuffer prefills and recovers from underrun", "[streaming][audio]")
{
    AudioPlaybackBuffer buffer(6, 4, 2);
    const std::array<float, 4> first = {0.1f, -0.1f, 0.2f, -0.2f};
    CHECK(buffer.Push(first.data(), 2) == 0);

    std::array<float, 6> output = {};
    CHECK(buffer.Read(output.data(), 3) == 0);
    CHECK(output == std::array<float, 6>{});
    CHECK(buffer.BufferedFrames() == 2);

    const std::array<float, 4> second = {0.3f, -0.3f, 0.4f, -0.4f};
    CHECK(buffer.Push(second.data(), 2) == 0);
    CHECK(buffer.Read(output.data(), 3) == 3);
    CHECK(output == std::array<float, 6>{0.1f, -0.1f, 0.2f, -0.2f, 0.3f, -0.3f});

    CHECK(buffer.Read(output.data(), 3) == 1);
    CHECK(output == std::array<float, 6>{0.4f, -0.4f, 0.0f, 0.0f, 0.0f, 0.0f});
    CHECK(buffer.UnderrunCount() == 1);

    buffer.Reset();
    CHECK(buffer.BufferedFrames() == 0);
    CHECK(buffer.UnderrunCount() == 0);
}

TEST_CASE("AudioPlaybackBuffer rejects excess frames without exceeding capacity",
          "[streaming][audio]")
{
    AudioPlaybackBuffer buffer(3, 1, 2);
    const std::array<float, 8> samples = {
        0.1f, -0.1f,
        0.2f, -0.2f,
        0.3f, -0.3f,
        0.4f, -0.4f,
    };

    CHECK(buffer.Push(samples.data(), 4) == 1);
    CHECK(buffer.BufferedFrames() == 3);
}
