// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <Protocol.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace oxr
{
namespace protocol
{

struct DecodedAudioPayload
{
    TcpAudioHeader header = {};
    std::vector<float> samples;
};

inline bool BuildTcpAudioPayload(int64_t presentationTimeNs,
                                 uint32_t frameCount,
                                 uint32_t sampleRateHz,
                                 uint16_t channels,
                                 const float* samples,
                                 size_t sampleCount,
                                 std::vector<uint8_t>& payload)
{
    if (frameCount == 0 || sampleRateHz == 0 || channels == 0 || samples == nullptr ||
        sampleCount != static_cast<size_t>(frameCount) * channels ||
        sampleCount > std::numeric_limits<uint32_t>::max() / sizeof(float))
    {
        payload.clear();
        return false;
    }

    const size_t sampleBytes = sampleCount * sizeof(float);
    if (sizeof(TcpAudioHeader) + sampleBytes > TCP_MAX_RECORD_PAYLOAD)
    {
        payload.clear();
        return false;
    }

    TcpAudioHeader header = {};
    header.presentationTimeNs = presentationTimeNs;
    header.frameCount = frameCount;
    header.payloadSize = static_cast<uint32_t>(sampleBytes);
    header.sampleRateHz = sampleRateHz;
    header.channels = channels;
    header.format = static_cast<uint16_t>(AudioSampleFormat::Float32);

    payload.resize(sizeof(header) + sampleBytes);
    memcpy(payload.data(), &header, sizeof(header));
    memcpy(payload.data() + sizeof(header), samples, sampleBytes);
    return true;
}

inline bool ParseTcpAudioPayload(const uint8_t* payload,
                                 size_t payloadSize,
                                 DecodedAudioPayload& decoded)
{
    decoded = {};
    if (payload == nullptr || payloadSize < sizeof(TcpAudioHeader))
    {
        return false;
    }

    memcpy(&decoded.header, payload, sizeof(decoded.header));
    if (decoded.header.frameCount == 0 || decoded.header.sampleRateHz == 0 ||
        decoded.header.channels == 0 ||
        decoded.header.format != static_cast<uint16_t>(AudioSampleFormat::Float32))
    {
        decoded = {};
        return false;
    }

    const size_t expectedSampleCount =
        static_cast<size_t>(decoded.header.frameCount) * decoded.header.channels;
    if (expectedSampleCount > std::numeric_limits<uint32_t>::max() / sizeof(float))
    {
        decoded = {};
        return false;
    }
    const size_t expectedSampleBytes = expectedSampleCount * sizeof(float);
    if (decoded.header.payloadSize != expectedSampleBytes ||
        payloadSize != sizeof(TcpAudioHeader) + expectedSampleBytes)
    {
        decoded = {};
        return false;
    }

    decoded.samples.resize(expectedSampleCount);
    memcpy(decoded.samples.data(), payload + sizeof(TcpAudioHeader), expectedSampleBytes);
    return true;
}

} // namespace protocol
} // namespace oxr
