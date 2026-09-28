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

// Fronteira para inicialização do jogo. A interface trabalha apenas com este
// contrato e não conhece detalhes dos argumentos Java ou do perfil Forge.
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
