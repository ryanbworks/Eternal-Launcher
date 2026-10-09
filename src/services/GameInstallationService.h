#pragma once

#include "services/IGameInstallationService.h"
#include "config/LauncherSettings.h"
#include <utility>

#include <filesystem>

// Orquestra Mojang, Forge e o manifesto privado, mantendo cada integração em
// um serviço menor e testável.
class GameInstallationService final : public IGameInstallationService
{
public:
    explicit GameInstallationService(LauncherSettings settings = {}) : settings_(std::move(settings)) {}
    GameInstallationResult ensureInstalled(
        std::wstring_view privateManifest,
        std::stop_token stopToken,
        const GameInstallationProgressCallback& progress) override;

private:
    GameInstallationResult install(std::wstring_view privateManifest,std::stop_token stopToken,
        const GameInstallationProgressCallback& progress);
    LauncherSettings settings_;
    [[nodiscard]] std::filesystem::path dataDirectory() const;
    [[nodiscard]] std::filesystem::path gameDirectory() const;
};
