#pragma once

#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

struct GameInstallationProgress
{
    int percent = 0;
    std::wstring message;
};

struct GameInstallationResult
{
    bool success = false;
    std::wstring message;
    std::size_t synchronizedFiles = 0;
};

using GameInstallationProgressCallback =
    std::function<void(int percent, std::wstring_view message)>;

// Contrato do instalador completo da instância Minecraft mantida pelo launcher.
class IGameInstallationService
{
public:
    virtual ~IGameInstallationService() = default;
    virtual GameInstallationResult ensureInstalled(
        std::wstring_view privateManifest,
        std::stop_token stopToken,
        const GameInstallationProgressCallback& progress) = 0;
};
