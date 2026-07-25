// SPDX-License-Identifier: MPL-2.0

#include "AudioPlaybackBuffer.h"

#include <algorithm>
#include <cstddef>

AudioPlaybackBuffer::AudioPlaybackBuffer(uint32_t capacityFrames,
                                         uint32_t startThresholdFrames,
                                         uint32_t channels)
    : capacityFrames_(std::max(capacityFrames, 1u)),
      startThresholdFrames_(std::clamp(startThresholdFrames, 1u,
                                      std::max(capacityFrames, 1u))),
      channels_(std::max(channels, 1u)),
      samples_(static_cast<size_t>(std::max(capacityFrames, 1u)) *
               std::max(channels, 1u))
{
}

uint32_t AudioPlaybackBuffer::Push(const float* samples, uint32_t frameCount)
{
    if (samples == nullptr || frameCount == 0)
    {
        return frameCount;
    }

    const uint64_t write = writePosition_.load(std::memory_order_relaxed);
    const uint64_t read = readPosition_.load(std::memory_order_acquire);
    const uint64_t buffered = write - read;
    const uint32_t freeFrames = buffered >= capacityFrames_
        ? 0
        : capacityFrames_ - static_cast<uint32_t>(buffered);
    const uint32_t acceptedFrames = std::min(frameCount, freeFrames);

    for (uint32_t frame = 0; frame < acceptedFrames; ++frame)
    {
        const size_t destinationFrame =
            static_cast<size_t>((write + frame) % capacityFrames_);
        for (uint32_t channel = 0; channel < channels_; ++channel)
        {
            samples_[destinationFrame * channels_ + channel] =
                samples[static_cast<size_t>(frame) * channels_ + channel];
        }
    }
    writePosition_.store(write + acceptedFrames, std::memory_order_release);
    return frameCount - acceptedFrames;
}

uint32_t AudioPlaybackBuffer::Read(float* output, uint32_t frameCount)
{
    if (output == nullptr || frameCount == 0)
    {
        return 0;
    }

    std::fill(output, output + static_cast<size_t>(frameCount) * channels_, 0.0f);

    const uint64_t read = readPosition_.load(std::memory_order_relaxed);
    const uint64_t write = writePosition_.load(std::memory_order_acquire);
    const uint32_t bufferedFrames =
        static_cast<uint32_t>(std::min<uint64_t>(write - read, capacityFrames_));
    if (!started_.load(std::memory_order_relaxed))
    {
        if (bufferedFrames < startThresholdFrames_)
        {
            return 0;
        }
        started_.store(true, std::memory_order_relaxed);
    }

    const uint32_t copiedFrames = std::min(frameCount, bufferedFrames);
    for (uint32_t frame = 0; frame < copiedFrames; ++frame)
    {
        const size_t sourceFrame =
            static_cast<size_t>((read + frame) % capacityFrames_);
        for (uint32_t channel = 0; channel < channels_; ++channel)
        {
            output[static_cast<size_t>(frame) * channels_ + channel] =
                samples_[sourceFrame * channels_ + channel];
        }
    }
    readPosition_.store(read + copiedFrames, std::memory_order_release);

    if (copiedFrames < frameCount)
    {
        underrunCount_.fetch_add(1, std::memory_order_relaxed);
        started_.store(false, std::memory_order_relaxed);
    }
    return copiedFrames;
}

void AudioPlaybackBuffer::Reset()
{
    readPosition_.store(0, std::memory_order_relaxed);
    writePosition_.store(0, std::memory_order_relaxed);
    underrunCount_.store(0, std::memory_order_relaxed);
    started_.store(false, std::memory_order_relaxed);
}

uint32_t AudioPlaybackBuffer::BufferedFrames() const
{
    const uint64_t read = readPosition_.load(std::memory_order_acquire);
    const uint64_t write = writePosition_.load(std::memory_order_acquire);
    return static_cast<uint32_t>(std::min<uint64_t>(write - read, capacityFrames_));
}

uint32_t AudioPlaybackBuffer::UnderrunCount() const
{
    return underrunCount_.load(std::memory_order_relaxed);
}
