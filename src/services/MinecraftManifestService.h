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

// Interpreta os manifestos oficiais da Mojang e converte o perfil 1.20.1 em
// uma lista explícita de downloads, nativas e entradas de classpath.
class MinecraftManifestService
{
public:
    [[nodiscard]] MinecraftInstallPlanResult createPlan(
        const std::filesystem::path& gameDirectory, std::stop_token stop = {}) const;
};
