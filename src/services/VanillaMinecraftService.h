#pragma once

#include "types/InstallationProgress.h"

#include <filesystem>
#include <stop_token>

// Prepares an unmodified Minecraft 1.8.9 with its own Java 8 and its own folder, so it
// never touches the group's Forge instance or depends on the file server.
class VanillaMinecraftService
{
public:
    [[nodiscard]] static std::filesystem::path gameDirectory();
    [[nodiscard]] static std::filesystem::path javaExecutable();
    [[nodiscard]] static bool isInstalled();
    [[nodiscard]] InstallationResult prepare(std::stop_token stop, const InstallationProgressCallback& progress) const;
};
