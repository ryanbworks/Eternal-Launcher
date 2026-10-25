#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include "types/MinecraftIdentity.h"
#include "config/LauncherSettings.h"

// Builds the command line specific to Minecraft 1.20.1 with Forge 47.4.10.
// Separating this responsibility keeps the process service focused on validating
// the installation, starting Java and monitoring the game.
class MinecraftLaunchCommandBuilder
{
public:
    [[nodiscard]] std::wstring build(
        std::wstring_view nickname,
        const std::filesystem::path& javaExecutable,
        const std::filesystem::path& gameDirectory) const;
    [[nodiscard]] std::wstring build(
        const MinecraftIdentity& identity,
        const std::filesystem::path& javaExecutable,
        const std::filesystem::path& gameDirectory) const;
    [[nodiscard]] std::wstring build(
        const MinecraftIdentity& identity, const std::filesystem::path& javaExecutable,
        const std::filesystem::path& gameDirectory, const LauncherSettings& settings) const;

private:
    [[nodiscard]] std::wstring buildClasspath(
        const std::filesystem::path& gameDirectory) const;
};
