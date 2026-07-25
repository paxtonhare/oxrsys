// SPDX-License-Identifier: MPL-2.0

#import "AudioCapture.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{

constexpr uint32_t kCaptureSampleRateHz = 48'000;
constexpr uint16_t kCaptureChannels = 2;

int64_t SteadyClockNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

float ReadLinearPcmSample(const AudioBufferList* buffers,
                          const AudioStreamBasicDescription& format,
                          uint32_t frame,
                          uint32_t channel)
{
    const bool nonInterleaved =
        (format.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
    const uint32_t sourceChannel =
        std::min(channel, std::max(format.mChannelsPerFrame, 1u) - 1);
    const uint32_t bufferIndex = nonInterleaved ? sourceChannel : 0;
    if (bufferIndex >= buffers->mNumberBuffers ||
        buffers->mBuffers[bufferIndex].mData == nullptr)
    {
        return 0.0f;
    }

    const uint32_t channelInBuffer = nonInterleaved ? 0 : sourceChannel;
    const size_t sampleIndex =
        static_cast<size_t>(frame) * (nonInterleaved ? 1 : format.mChannelsPerFrame) +
        channelInBuffer;
    const AudioBuffer& buffer = buffers->mBuffers[bufferIndex];

    if ((format.mFormatFlags & kAudioFormatFlagIsFloat) != 0 &&
        format.mBitsPerChannel == 32 &&
        (sampleIndex + 1) * sizeof(float) <= buffer.mDataByteSize)
    {
        return static_cast<const float*>(buffer.mData)[sampleIndex];
    }
    if ((format.mFormatFlags & kAudioFormatFlagIsSignedInteger) != 0 &&
        format.mBitsPerChannel == 16 &&
        (sampleIndex + 1) * sizeof(int16_t) <= buffer.mDataByteSize)
    {
        return static_cast<float>(
            static_cast<const int16_t*>(buffer.mData)[sampleIndex]) / 32768.0f;
    }
    if ((format.mFormatFlags & kAudioFormatFlagIsSignedInteger) != 0 &&
        format.mBitsPerChannel == 32 &&
        (sampleIndex + 1) * sizeof(int32_t) <= buffer.mDataByteSize)
    {
        return static_cast<float>(
            static_cast<const int32_t*>(buffer.mData)[sampleIndex]) / 2147483648.0f;
    }
    return 0.0f;
}

} // namespace

@class OXRSAudioStreamOutput;

struct AudioCapture::Impl : public std::enable_shared_from_this<AudioCapture::Impl>
{
    ~Impl()
    {
        Stop();
        if (sampleQueue != nullptr)
        {
            dispatch_release(sampleQueue);
            sampleQueue = nullptr;
        }
    }

    bool Start(SamplesCallback nextCallback)
    {
        if (@available(macOS 13.0, *))
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (startRequested || running.load())
            {
                return false;
            }
            callback = std::move(nextCallback);
            if (!callback)
            {
                return false;
            }
            stopping.store(false);
            startRequested = true;
            if (sampleQueue == nullptr)
            {
                sampleQueue = dispatch_queue_create(
                    "net.demonixis.oxrsys.audio-capture", DISPATCH_QUEUE_SERIAL);
            }

            std::shared_ptr<Impl> self = shared_from_this();
            [SCShareableContent
                getShareableContentExcludingDesktopWindows:YES
                onScreenWindowsOnly:NO
                completionHandler:^(SCShareableContent* content, NSError* error) {
                    self->ConfigureAndStart(content, error);
                }];
            return true;
        }
        return false;
    }

    void ConfigureAndStart(SCShareableContent* content, NSError* error);
    void HandleSample(CMSampleBufferRef sampleBuffer);

    void Stop()
    {
        SCStream* streamToStop = nil;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            stopping.store(true);
            startRequested = false;
            callback = {};
            streamToStop = [stream retain];
        }

        if (streamToStop != nil)
        {
            dispatch_semaphore_t stoppedSemaphore = dispatch_semaphore_create(0);
            [streamToStop stopCaptureWithCompletionHandler:^(NSError*) {
                dispatch_semaphore_signal(stoppedSemaphore);
            }];
            dispatch_semaphore_wait(
                stoppedSemaphore,
                dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
            dispatch_release(stoppedSemaphore);
            [streamToStop release];
        }

        std::lock_guard<std::mutex> lock(stateMutex);
        running.store(false);
        if (stream != nil)
        {
            [stream release];
            stream = nil;
        }
        if (output != nil)
        {
            [output release];
            output = nil;
        }
    }

    mutable std::mutex stateMutex;
    SamplesCallback callback;
    std::atomic<bool> running{false};
    std::atomic<bool> stopping{false};
    bool startRequested = false;
    dispatch_queue_t sampleQueue = nullptr;
    SCStream* stream = nil;
    OXRSAudioStreamOutput* output = nil;
};

@interface OXRSAudioStreamOutput : NSObject <SCStreamOutput, SCStreamDelegate>
{
@public
    AudioCapture::Impl* captureImpl;
}
- (instancetype)initWithCaptureImpl:(AudioCapture::Impl*)impl;
@end

@implementation OXRSAudioStreamOutput

- (instancetype)initWithCaptureImpl:(AudioCapture::Impl*)impl
{
    self = [super init];
    if (self != nil)
    {
        captureImpl = impl;
    }
    return self;
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                  ofType:(SCStreamOutputType)type
{
    (void)stream;
    if (type == SCStreamOutputTypeAudio && captureImpl != nullptr)
    {
        captureImpl->HandleSample(sampleBuffer);
    }
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
    (void)stream;
    if (captureImpl != nullptr)
    {
        captureImpl->running.store(false);
    }
    spdlog::error("AudioCapture: ScreenCaptureKit stopped: {}",
                  error.localizedDescription.UTF8String ?: "unknown error");
}

@end

void AudioCapture::Impl::ConfigureAndStart(SCShareableContent* content, NSError* error)
{
    if (error != nil || content == nil)
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        startRequested = false;
        spdlog::error("AudioCapture: could not enumerate capture content: {}",
                      error.localizedDescription.UTF8String ?: "unknown error");
        return;
    }

    SCRunningApplication* currentApplication = nil;
    for (SCRunningApplication* application in content.applications)
    {
        if (application.processID == getpid())
        {
            currentApplication = application;
            break;
        }
    }
    if (currentApplication == nil || content.displays.count == 0)
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        startRequested = false;
        spdlog::error("AudioCapture: current application or display is not shareable");
        return;
    }

    SCContentFilter* filter = [[SCContentFilter alloc]
        initWithDisplay:content.displays.firstObject
        includingApplications:@[ currentApplication ]
        exceptingWindows:@[]];
    SCStreamConfiguration* configuration = [[SCStreamConfiguration alloc] init];
    configuration.capturesAudio = YES;
    configuration.sampleRate = kCaptureSampleRateHz;
    configuration.channelCount = kCaptureChannels;
    configuration.excludesCurrentProcessAudio = NO;
    configuration.width = 2;
    configuration.height = 2;
    configuration.minimumFrameInterval = CMTimeMake(1, 1);
    configuration.queueDepth = 1;
    configuration.showsCursor = NO;

    OXRSAudioStreamOutput* nextOutput =
        [[OXRSAudioStreamOutput alloc] initWithCaptureImpl:this];
    SCStream* nextStream =
        [[SCStream alloc] initWithFilter:filter configuration:configuration delegate:nextOutput];
    [configuration release];
    [filter release];

    NSError* addError = nil;
    const BOOL added = [nextStream addStreamOutput:nextOutput
                                              type:SCStreamOutputTypeAudio
                                sampleHandlerQueue:sampleQueue
                                             error:&addError];
    if (!added)
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        startRequested = false;
        spdlog::error("AudioCapture: could not attach audio output: {}",
                      addError.localizedDescription.UTF8String ?: "unknown error");
        [nextStream release];
        [nextOutput release];
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (stopping.load())
        {
            startRequested = false;
            [nextStream release];
            [nextOutput release];
            return;
        }
        stream = nextStream;
        output = nextOutput;
    }

    std::shared_ptr<Impl> self = shared_from_this();
    [nextStream startCaptureWithCompletionHandler:^(NSError* startError) {
        std::lock_guard<std::mutex> lock(self->stateMutex);
        self->startRequested = false;
        if (startError != nil || self->stopping.load())
        {
            self->running.store(false);
            spdlog::error("AudioCapture: app-audio capture failed to start: {}",
                          startError.localizedDescription.UTF8String ?: "capture stopped");
            return;
        }
        self->running.store(true);
        spdlog::info("AudioCapture: capturing current-app audio at 48 kHz stereo");
    }];
}

void AudioCapture::Impl::HandleSample(CMSampleBufferRef sampleBuffer)
{
    if (!running.load() || stopping.load() || sampleBuffer == nullptr ||
        !CMSampleBufferDataIsReady(sampleBuffer))
    {
        return;
    }

    const CMAudioFormatDescriptionRef formatDescription =
        static_cast<CMAudioFormatDescriptionRef>(
            CMSampleBufferGetFormatDescription(sampleBuffer));
    const AudioStreamBasicDescription* format =
        formatDescription != nullptr
            ? CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription)
            : nullptr;
    if (format == nullptr || format->mFormatID != kAudioFormatLinearPCM ||
        format->mChannelsPerFrame == 0)
    {
        return;
    }

    size_t bufferListSize = 0;
    CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, &bufferListSize, nullptr, 0, nullptr, nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, nullptr);
    if (bufferListSize < sizeof(AudioBufferList))
    {
        return;
    }

    std::vector<uint8_t> bufferListStorage(bufferListSize);
    auto* bufferList = reinterpret_cast<AudioBufferList*>(bufferListStorage.data());
    CMBlockBufferRef blockBuffer = nullptr;
    const OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, nullptr, bufferList, bufferListStorage.size(),
        kCFAllocatorDefault, kCFAllocatorDefault,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &blockBuffer);
    if (status != noErr || blockBuffer == nullptr)
    {
        return;
    }

    const CMItemCount sampleCount = CMSampleBufferGetNumSamples(sampleBuffer);
    if (sampleCount <= 0 || sampleCount > std::numeric_limits<uint32_t>::max())
    {
        CFRelease(blockBuffer);
        return;
    }

    const uint32_t frameCount = static_cast<uint32_t>(sampleCount);
    std::vector<float> interleaved(static_cast<size_t>(frameCount) * kCaptureChannels);
    for (uint32_t frame = 0; frame < frameCount; ++frame)
    {
        interleaved[static_cast<size_t>(frame) * 2] =
            ReadLinearPcmSample(bufferList, *format, frame, 0);
        interleaved[static_cast<size_t>(frame) * 2 + 1] =
            ReadLinearPcmSample(bufferList, *format, frame, 1);
    }
    CFRelease(blockBuffer);

    CMTime presentationTime = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    int64_t presentationTimeNs = SteadyClockNowNs();
    if (CMTIME_IS_NUMERIC(presentationTime) && presentationTime.timescale != 0)
    {
        const CMTime nanoseconds = CMTimeConvertScale(
            presentationTime, 1'000'000'000, kCMTimeRoundingMethod_Default);
        presentationTimeNs = nanoseconds.value;
    }

    SamplesCallback currentCallback;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        currentCallback = callback;
    }
    if (currentCallback)
    {
        currentCallback(interleaved.data(), frameCount, kCaptureSampleRateHz,
                        kCaptureChannels, presentationTimeNs);
    }
}

AudioCapture::AudioCapture()
    : impl_(std::make_shared<Impl>())
{
}

AudioCapture::~AudioCapture()
{
    Stop();
}

bool AudioCapture::IsSupported()
{
    if (@available(macOS 13.0, *))
    {
        return true;
    }
    return false;
}

bool AudioCapture::Start(SamplesCallback callback)
{
    return impl_->Start(std::move(callback));
}

void AudioCapture::Stop()
{
    if (impl_)
    {
        impl_->Stop();
    }
}

bool AudioCapture::IsRunning() const
{
    return impl_ && impl_->running.load();
}
