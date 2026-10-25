#pragma once

#include "services/VerifiedDownloader.h"

#include <filesystem>
#include <string>
#include <vector>

struct MinecraftInstallPlan
{
    std::vector<VerifiedDownload> downloads;
    std::vector<std::filesystem::path> nativeArchives;
    std::vector<std::filesystem::path> classpathEntries;
    std::string versionMetadata;
    std::string assetIndexMetadata;
};

struct MinecraftInstallPlanResult
{
    bool success = false;
    std::wstring message;
    MinecraftInstallPlan plan;
    InstallationFailure failure = InstallationFailure::None;
};

// Parses Mojang's official manifests and turns the 1.20.1 profile into
// an explicit list of downloads, natives and classpath entries.
class MinecraftManifestService
{
public:
    [[nodiscard]] MinecraftInstallPlanResult createPlan(
        const std::filesystem::path& gameDirectory, std::stop_token stop = {}) const;
};
