// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "Protocol.h"

#include <cstdint>

namespace oxr::protocol
{

struct ControllerButtonInputs
{
    bool a = false;
    bool b = false;
    bool x = false;
    bool y = false;
    bool menu = false;
    bool leftThumbstick = false;
    bool rightThumbstick = false;
};

constexpr uint32_t EncodeControllerButtonInputs(const ControllerButtonInputs& inputs)
{
    return (inputs.a ? BUTTON_A : 0u) |
           (inputs.b ? BUTTON_B : 0u) |
           (inputs.x ? BUTTON_X : 0u) |
           (inputs.y ? BUTTON_Y : 0u) |
           (inputs.menu ? BUTTON_MENU : 0u) |
           (inputs.leftThumbstick ? BUTTON_LEFT_THUMBSTICK : 0u) |
           (inputs.rightThumbstick ? BUTTON_RIGHT_THUMBSTICK : 0u);
}

} // namespace oxr::protocol
