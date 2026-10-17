// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace oxr::display_refresh
{

inline uint32_t SelectAdvertisedRefreshRate(float requestedRateHz,
                                            bool requestAccepted,
                                            float activeRateHz,
                                            uint32_t fallbackRateHz)
{
    const float selectedRateHz = requestAccepted
        ? requestedRateHz
        : (activeRateHz > 0.0f ? activeRateHz : static_cast<float>(fallbackRateHz));
    return static_cast<uint32_t>(std::max(1.0f, std::round(selectedRateHz)));
}

} // namespace oxr::display_refresh
