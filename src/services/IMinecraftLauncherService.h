#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include "types/MinecraftIdentity.h"

enum class MinecraftLaunchIssue
{
    None,
    GameAlreadyRunning,
    InvalidNickname,
    InvalidConfiguration,
    JavaMissing,
    InstallationIncomplete,
    NativeLibrariesMissing,
    ProcessCreationFailed
};

struct MinecraftLaunchResult
{
    bool success = false;
    std::wstring message;
    unsigned long processId = 0;
    MinecraftLaunchIssue issue = MinecraftLaunchIssue::None;
};

// Boundary for launching the game. The interface only works with this
// contract and knows nothing about the Java arguments or the Forge profile.
class IMinecraftLauncherService
{
public:
    virtual ~IMinecraftLauncherService() = default;

    [[nodiscard]] virtual MinecraftLaunchResult validateInstallation() const = 0;
    virtual MinecraftLaunchResult launch(std::wstring_view nickname) = 0;
    virtual MinecraftLaunchResult launch(const MinecraftIdentity& identity) = 0;
    [[nodiscard]] virtual bool isGameRunning() const = 0;
    [[nodiscard]] virtual std::filesystem::path gameDirectory() const = 0;
};
