#pragma once

#include <filesystem>
#include <string>

struct GameServerSettings
{
    std::wstring name;
    std::wstring address;
};

// Lê a configuração editável do servidor de jogo, separada do servidor HTTP.
class GameServerSettingsStore
{
public:
    [[nodiscard]] static std::filesystem::path filePath(
        const std::filesystem::path& dataDirectory);
    [[nodiscard]] static bool loadOrCreate(
        const std::filesystem::path& dataDirectory,
        GameServerSettings& settings,
        std::wstring& errorMessage);
};
