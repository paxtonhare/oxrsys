// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

struct StreamingAudioFrame
{
    std::vector<float> samples;
    int64_t presentationTimeNs = 0;
    uint32_t frameIndex = 0;
    uint32_t frameCount = 0;
};

class StreamingAudioQueue
{
public:
    explicit StreamingAudioQueue(size_t capacity);

    void Start();
    void Stop();
    bool Push(StreamingAudioFrame frame);
    bool WaitPop(const std::atomic<bool>& running, StreamingAudioFrame& outFrame);
    void Clear();
    size_t Size() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable readyCv_;
    std::deque<StreamingAudioFrame> frames_;
    size_t capacity_ = 1;
    bool stopped_ = true;
};
