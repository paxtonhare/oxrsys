// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

class AudioPlaybackBuffer
{
public:
    AudioPlaybackBuffer(uint32_t capacityFrames,
                        uint32_t startThresholdFrames,
                        uint32_t channels);

    uint32_t Push(const float* samples, uint32_t frameCount);
    uint32_t Read(float* output, uint32_t frameCount);
    void Reset();

    uint32_t BufferedFrames() const;
    uint32_t UnderrunCount() const;

private:
    std::vector<float> samples_;
    uint32_t capacityFrames_ = 1;
    uint32_t startThresholdFrames_ = 1;
    uint32_t channels_ = 2;
    std::atomic<uint64_t> readPosition_{0};
    std::atomic<uint64_t> writePosition_{0};
    std::atomic<uint32_t> underrunCount_{0};
    std::atomic<bool> started_{false};
};
