#pragma once

#include "services/IMinecraftLauncherService.h"

#include <filesystem>
#include <windows.h>

// Implementa a criação do processo Java para uma instância Forge já preparada.
// Instalar Java, Minecraft, Forge e mods permanece responsabilidade de serviços futuros.
class MinecraftLauncherService final : public IMinecraftLauncherService
{
public:
    MinecraftLauncherService();
    ~MinecraftLauncherService() override;

    [[nodiscard]] MinecraftLaunchResult validateInstallation() const override;
    MinecraftLaunchResult launch(std::wstring_view nickname) override;
    [[nodiscard]] bool isGameRunning() const override;
    [[nodiscard]] std::filesystem::path gameDirectory() const override;

private:
    std::filesystem::path resolveDataDirectory() const;
    std::filesystem::path findJavaExecutable() const;
    HANDLE processHandle_ = nullptr;
    unsigned long processId_ = 0;
};
