#pragma once

#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include "types/InstallationProgress.h"

using GameInstallationProgress = InstallationProgress;

struct GameInstallationResult
{
    bool success = false;
    std::wstring message;
    std::size_t synchronizedFiles = 0;
    InstallationFailure failure = InstallationFailure::None;
    InstallationOutcome outcome = InstallationOutcome::Failed;
};

using GameInstallationProgressCallback = InstallationProgressCallback;

// Contract for the full installer of the Minecraft instance managed by the launcher.
class IGameInstallationService
{
public:
    virtual ~IGameInstallationService() = default;
    virtual GameInstallationResult ensureInstalled(
        std::wstring_view privateManifest,
        std::stop_token stopToken,
        const GameInstallationProgressCallback& progress) = 0;
};
