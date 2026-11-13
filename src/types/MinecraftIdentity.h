#pragma once

#include <string>

struct MinecraftIdentity
{
    std::wstring name;
    std::wstring uuid;
    std::wstring accessToken;
    std::wstring xuid;
    std::wstring clientId;
    // Conta Eternal: "-javaagent:" value that points the game at the account server
    // ("<authlib-injector.jar>=<base>/authlib-injector"). Empty for Microsoft accounts.
    std::wstring javaAgent;

    [[nodiscard]] bool online() const { return !accessToken.empty(); }
    [[nodiscard]] bool eternal() const { return online() && !javaAgent.empty(); }
};
