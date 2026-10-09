#pragma once
#include "config/LauncherConfig.h"
#include <filesystem>
#include <string>
#include <string_view>

struct LauncherSettings
{
    int minimumMemoryMegabytes = LauncherConfig::MinimumMemoryMegabytes;
    int maximumMemoryMegabytes = LauncherConfig::MaximumMemoryMegabytes;
    int windowWidth = 1280;
    int windowHeight = 720;
    bool fullscreen = false;
    bool minimizeOnLaunch = true;
    // Empty paths select the existing automatic/private defaults.
    std::filesystem::path javaExecutable;
    std::filesystem::path gameDirectory;
};

class LauncherSettingsStore
{
public:
    static std::filesystem::path dataDirectory();
    static std::filesystem::path filePath(const std::filesystem::path& directory);
    static std::filesystem::path effectiveGameDirectory(const LauncherSettings& settings);
    static LauncherSettings load(const std::filesystem::path& directory, std::wstring* warning = nullptr);
    static bool save(const std::filesystem::path& directory, const LauncherSettings& settings, std::wstring& error);
    static bool validate(const LauncherSettings& settings, std::wstring& error, bool checkPaths = true);
    static bool isJava17Executable(const std::filesystem::path& executable);
    static unsigned long long physicalMemoryMegabytes();
};
