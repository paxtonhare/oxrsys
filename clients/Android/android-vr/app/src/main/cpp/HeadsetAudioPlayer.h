// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "AudioPlaybackBuffer.h"

#include <aaudio/AAudio.h>

#include <atomic>
#include <cstdint>

class HeadsetAudioPlayer
{
public:
    HeadsetAudioPlayer();
    ~HeadsetAudioPlayer();

    HeadsetAudioPlayer(const HeadsetAudioPlayer&) = delete;
    HeadsetAudioPlayer& operator=(const HeadsetAudioPlayer&) = delete;

    bool Start(uint32_t sampleRateHz, uint16_t channels);
    void Stop();
    uint32_t Push(const float* samples, uint32_t frameCount);
    bool IsRunning() const;
    uint32_t BufferedFrames() const;
    uint32_t UnderrunCount() const;

private:
    static aaudio_data_callback_result_t DataCallback(
        AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);
    static void ErrorCallback(
        AAudioStream* stream, void* userData, aaudio_result_t error);

    AudioPlaybackBuffer buffer_;
    AAudioStream* stream_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<bool> disconnected_{false};
};
