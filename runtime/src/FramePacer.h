// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <chrono>
#include <optional>

class FramePacer
{
public:
    using Clock = std::chrono::steady_clock;

    Clock::time_point NextDeadline(Clock::time_point now,
                                   std::chrono::nanoseconds framePeriod)
    {
        if (framePeriod <= std::chrono::nanoseconds::zero())
        {
            return now;
        }

        if (!nextDeadline_.has_value() || framePeriod != framePeriod_)
        {
            framePeriod_ = framePeriod;
            nextDeadline_ = now + framePeriod;
        }
        else if (now > *nextDeadline_ + framePeriod * 4)
        {
            nextDeadline_ = now + framePeriod;
        }
        else
        {
            while (*nextDeadline_ <= now)
            {
                *nextDeadline_ += framePeriod;
            }
        }

        const Clock::time_point deadline = *nextDeadline_;
        *nextDeadline_ += framePeriod;
        return deadline;
    }

    void Reset()
    {
        nextDeadline_.reset();
        framePeriod_ = std::chrono::nanoseconds::zero();
    }

private:
    std::optional<Clock::time_point> nextDeadline_;
    std::chrono::nanoseconds framePeriod_ = std::chrono::nanoseconds::zero();
};
