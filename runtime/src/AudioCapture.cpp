// SPDX-License-Identifier: MPL-2.0

#include "AudioCapture.h"

struct AudioCapture::Impl
{
};

AudioCapture::AudioCapture()
    : impl_(std::make_shared<Impl>())
{
}

AudioCapture::~AudioCapture() = default;

bool AudioCapture::IsSupported()
{
    return false;
}

bool AudioCapture::Start(SamplesCallback)
{
    return false;
}

void AudioCapture::Stop()
{
}

bool AudioCapture::IsRunning() const
{
    return false;
}
