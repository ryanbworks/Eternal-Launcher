#pragma once

#include "services/IMinecraftLauncherService.h"

#include <filesystem>
#include <windows.h>

// Valida a instância preparada e cria o processo Java com o perfil Forge.
// Downloads e instalação permanecem isolados nos serviços próprios.
class MinecraftLauncherService final : public IMinecraftLauncherService
{
public:
    MinecraftLauncherService();
    ~MinecraftLauncherService() override;

    [[nodiscard]] MinecraftLaunchResult validateInstallation() const override;
    MinecraftLaunchResult launch(std::wstring_view nickname) override;
    MinecraftLaunchResult launch(const MinecraftIdentity& identity) override;
    [[nodiscard]] bool isGameRunning() const override;
    [[nodiscard]] unsigned long processExitCode() const;
    [[nodiscard]] std::filesystem::path gameDirectory() const override;

private:
    std::filesystem::path resolveDataDirectory() const;
    std::filesystem::path findJavaExecutable() const;
    HANDLE processHandle_ = nullptr;
    unsigned long processId_ = 0;
};
