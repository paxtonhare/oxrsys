// SPDX-License-Identifier: MPL-2.0

#include "HeadsetAudioPlayer.h"

#include <android/log.h>

#include <algorithm>

namespace
{

constexpr uint32_t kChannels = 2;
constexpr uint32_t kBufferCapacityFrames = 4'800; // 100 ms at 48 kHz
constexpr uint32_t kStartThresholdFrames = 1'440; // 30 ms prefill

} // namespace

HeadsetAudioPlayer::HeadsetAudioPlayer()
    : buffer_(kBufferCapacityFrames, kStartThresholdFrames, kChannels)
{
}

HeadsetAudioPlayer::~HeadsetAudioPlayer()
{
    Stop();
}

bool HeadsetAudioPlayer::Start(uint32_t sampleRateHz, uint16_t channels)
{
    Stop();
    if (sampleRateHz != 48'000 || channels != kChannels)
    {
        return false;
    }

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || builder == nullptr)
    {
        return false;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setSampleRate(builder, static_cast<int32_t>(sampleRateHz));
    AAudioStreamBuilder_setChannelCount(builder, channels);
    AAudioStreamBuilder_setBufferCapacityInFrames(builder, kBufferCapacityFrames);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_GAME);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);
    AAudioStreamBuilder_setDataCallback(builder, &HeadsetAudioPlayer::DataCallback, this);
    AAudioStreamBuilder_setErrorCallback(builder, &HeadsetAudioPlayer::ErrorCallback, this);

    result = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK || stream_ == nullptr)
    {
        stream_ = nullptr;
        return false;
    }

    if (AAudioStream_getSampleRate(stream_) != static_cast<int32_t>(sampleRateHz) ||
        AAudioStream_getChannelCount(stream_) != channels ||
        AAudioStream_getFormat(stream_) != AAUDIO_FORMAT_PCM_FLOAT)
    {
        AAudioStream_close(stream_);
        stream_ = nullptr;
        return false;
    }

    const int32_t framesPerBurst = AAudioStream_getFramesPerBurst(stream_);
    if (framesPerBurst > 0)
    {
        AAudioStream_setBufferSizeInFrames(
            stream_, std::min(framesPerBurst * 2,
                              AAudioStream_getBufferCapacityInFrames(stream_)));
    }

    buffer_.Reset();
    disconnected_.store(false);
    running_.store(true);
    result = AAudioStream_requestStart(stream_);
    if (result != AAUDIO_OK)
    {
        running_.store(false);
        AAudioStream_close(stream_);
        stream_ = nullptr;
        return false;
    }

    __android_log_print(
        ANDROID_LOG_INFO, "OXRSys",
        "Headset audio started: %d Hz, %d channels, burst=%d frames",
        AAudioStream_getSampleRate(stream_),
        AAudioStream_getChannelCount(stream_),
        framesPerBurst);
    return true;
}

void HeadsetAudioPlayer::Stop()
{
    running_.store(false);
    if (stream_ != nullptr)
    {
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
    disconnected_.store(false);
    buffer_.Reset();
}

uint32_t HeadsetAudioPlayer::Push(const float* samples, uint32_t frameCount)
{
    if (!running_.load() || disconnected_.load())
    {
        return frameCount;
    }
    return buffer_.Push(samples, frameCount);
}

bool HeadsetAudioPlayer::IsRunning() const
{
    return running_.load() && !disconnected_.load();
}

uint32_t HeadsetAudioPlayer::BufferedFrames() const
{
    return buffer_.BufferedFrames();
}

uint32_t HeadsetAudioPlayer::UnderrunCount() const
{
    return buffer_.UnderrunCount();
}

aaudio_data_callback_result_t HeadsetAudioPlayer::DataCallback(
    AAudioStream*, void* userData, void* audioData, int32_t numFrames)
{
    auto* player = static_cast<HeadsetAudioPlayer*>(userData);
    if (player == nullptr || audioData == nullptr || numFrames <= 0 ||
        !player->running_.load() || player->disconnected_.load())
    {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    player->buffer_.Read(static_cast<float*>(audioData),
                         static_cast<uint32_t>(numFrames));
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void HeadsetAudioPlayer::ErrorCallback(
    AAudioStream*, void* userData, aaudio_result_t error)
{
    auto* player = static_cast<HeadsetAudioPlayer*>(userData);
    if (player != nullptr && error == AAUDIO_ERROR_DISCONNECTED)
    {
        player->disconnected_.store(true);
        player->running_.store(false);
    }
}
