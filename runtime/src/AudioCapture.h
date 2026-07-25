// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <cstdint>
#include <functional>
#include <memory>

class AudioCapture
{
public:
    using SamplesCallback = std::function<void(const float* samples,
                                               uint32_t frameCount,
                                               uint32_t sampleRateHz,
                                               uint16_t channels,
                                               int64_t presentationTimeNs)>;

    AudioCapture();
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    static bool IsSupported();
    bool Start(SamplesCallback callback);
    void Stop();
    bool IsRunning() const;

    struct Impl;

private:
    std::shared_ptr<Impl> impl_;
};
