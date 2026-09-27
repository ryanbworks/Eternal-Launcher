#pragma once

#include "services/IGameInstallationService.h"

#include <filesystem>

// Orquestra Mojang, Forge e o manifesto privado, mantendo cada integração em
// um serviço menor e testável.
class GameInstallationService final : public IGameInstallationService
{
public:
    GameInstallationResult ensureInstalled(
        std::wstring_view privateManifest,
        std::stop_token stopToken,
        const GameInstallationProgressCallback& progress) override;

private:
    [[nodiscard]] std::filesystem::path dataDirectory() const;
    [[nodiscard]] std::filesystem::path gameDirectory() const;
};
