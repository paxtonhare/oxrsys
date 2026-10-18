// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstring>

// Historical device allowlist. Do not infer Main10 from the Quest family name.
inline bool HasVerifiedMain10Decoder(const char* systemName)
{
    return systemName != nullptr && std::strcmp(systemName, "Meta Quest 3") == 0;
}
