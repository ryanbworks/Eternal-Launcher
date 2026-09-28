#pragma once

#include <string>

struct MinecraftIdentity
{
    std::wstring name;
    std::wstring uuid;
    std::wstring accessToken;
    std::wstring xuid;
    std::wstring clientId;

    [[nodiscard]] bool online() const { return !accessToken.empty(); }
};
