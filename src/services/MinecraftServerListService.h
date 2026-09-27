#pragma once

#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>

// Mantém a entrada gerenciada pelo launcher na lista Multiplayer do Minecraft.
class MinecraftServerListService
{
public:
    [[nodiscard]] bool ensureServerPresent(
        const std::filesystem::path& gameDirectory,
        std::wstring_view serverName,
        std::wstring_view serverAddress,
        std::stop_token stopToken,
        std::wstring& errorMessage) const;
};
