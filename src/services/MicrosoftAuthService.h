#pragma once

#include "types/MinecraftIdentity.h"

#include <functional>
#include <stop_token>
#include <string>

struct MicrosoftAuthResult
{
    bool success = false;
    MinecraftIdentity identity;
    std::wstring message;
};

struct MicrosoftDeviceCode
{
    std::wstring url;
    std::wstring code;
};

class MicrosoftAuthService
{
public:
    [[nodiscard]] static std::wstring clientId();
    [[nodiscard]] MicrosoftAuthResult signIn(
        std::stop_token stopToken,
        const std::function<void(const MicrosoftDeviceCode&)>& onCode) const;
};
