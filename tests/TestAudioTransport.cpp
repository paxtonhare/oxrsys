// SPDX-License-Identifier: MPL-2.0

#include <catch2/catch_test_macros.hpp>

#include <AudioTransport.h>

#include <cstdint>
#include <vector>

TEST_CASE("TCP audio records preserve timing format and samples", "[streaming][audio]")
{
    const std::vector<float> samples = {
        -1.0f, 1.0f,
        -0.25f, 0.25f,
    };

    std::vector<uint8_t> payload;
    REQUIRE(oxr::protocol::BuildTcpAudioPayload(
        123'456'789, 2, 48'000, 2, samples.data(), samples.size(), payload));

    oxr::protocol::DecodedAudioPayload decoded = {};
    REQUIRE(oxr::protocol::ParseTcpAudioPayload(payload.data(), payload.size(), decoded));
    CHECK(decoded.header.presentationTimeNs == 123'456'789);
    CHECK(decoded.header.frameCount == 2);
    CHECK(decoded.header.sampleRateHz == 48'000);
    CHECK(decoded.header.channels == 2);
    CHECK(decoded.header.format ==
          static_cast<uint16_t>(oxr::protocol::AudioSampleFormat::Float32));
    CHECK(decoded.samples == samples);

    payload.pop_back();
    CHECK(!oxr::protocol::ParseTcpAudioPayload(payload.data(), payload.size(), decoded));
}
