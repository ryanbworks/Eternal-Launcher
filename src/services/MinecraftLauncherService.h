#pragma once

#include "services/IMinecraftLauncherService.h"
#include "config/LauncherSettings.h"

#include <filesystem>
#include <windows.h>

// Validates the prepared instance and creates the Java process with the Forge profile.
// Downloads and installation stay isolated in their own services.
class MinecraftLauncherService final : public IMinecraftLauncherService
{
public:
    explicit MinecraftLauncherService(LauncherSettings settings = {});
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
    LauncherSettings settings_;
};
