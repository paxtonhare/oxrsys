// SPDX-License-Identifier: MPL-2.0

#include "StreamingAudioQueue.h"

#include <algorithm>
#include <utility>

StreamingAudioQueue::StreamingAudioQueue(size_t capacity)
    : capacity_(std::max(capacity, size_t{1}))
{
}

void StreamingAudioQueue::Start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = false;
}

void StreamingAudioQueue::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    readyCv_.notify_all();
}

bool StreamingAudioQueue::Push(StreamingAudioFrame frame)
{
    bool droppedOldest = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_)
        {
            return false;
        }
        if (frames_.size() == capacity_)
        {
            frames_.pop_front();
            droppedOldest = true;
        }
        frames_.push_back(std::move(frame));
    }
    readyCv_.notify_one();
    return droppedOldest;
}

bool StreamingAudioQueue::WaitPop(const std::atomic<bool>& running,
                                  StreamingAudioFrame& outFrame)
{
    std::unique_lock<std::mutex> lock(mutex_);
    readyCv_.wait(lock, [this, &running] {
        return !running.load() || stopped_ || !frames_.empty();
    });

    if (frames_.empty())
    {
        outFrame = {};
        return false;
    }

    outFrame = std::move(frames_.front());
    frames_.pop_front();
    return true;
}

void StreamingAudioQueue::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    frames_.clear();
}

size_t StreamingAudioQueue::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return frames_.size();
}
