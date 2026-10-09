#pragma once
#include "config/LauncherSettings.h"
#include "services/IFileServerService.h"
#include "services/IJavaRuntimeService.h"
#include "services/IGameInstallationService.h"
#include "services/IMinecraftLauncherService.h"

// Owns preparation only. The UI decides whether to launch after the terminal result.
class InstallationCoordinator {
public:
    InstallationCoordinator(IFileServerService& files,IJavaRuntimeService& java,
        IGameInstallationService& game,IMinecraftLauncherService& launcher,const LauncherSettings& settings)
        : files_(files),java_(java),game_(game),launcher_(launcher),settings_(settings) {}
    InstallationResult run(std::stop_token stop,const InstallationProgressCallback& progress) const;
    static void recordLaunchFailure(const LauncherSettings& settings,std::wstring_view detail);
private:
    IFileServerService& files_;
    IJavaRuntimeService& java_;
    IGameInstallationService& game_;
    IMinecraftLauncherService& launcher_;
    const LauncherSettings& settings_;
};
