#pragma once

#include "components/HeaderComponent.h"
#include "components/HeroComponent.h"
#include "components/NewsPanelComponent.h"
#include "components/PlayPanelComponent.h"
#include "components/PlayerProfileComponent.h"
#include "components/ProgressComponent.h"
#include "components/StatusPanelComponent.h"
#include "services/IFileServerService.h"
#include "services/IJavaRuntimeService.h"
#include "services/IMinecraftLauncherService.h"
#include "types/LauncherState.h"

#include <windows.h>

#include <memory>
#include <thread>

class MainPage
{
public:
    explicit MainPage(HINSTANCE instance);
    ~MainPage();

    bool create(int showCommand);

private:
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void paint();
    void enableDarkTitleBar() const;
    void createNicknameInput();
    void layoutNicknameInput() const;
    void handlePrimaryAction();
    void pollGameProcess();
    void showSettingsPlaceholder() const;
    void checkFileServerAsync();
    void applyFileServerResult(const FileServerManifestResponse& response);
    void startJavaRuntimeInstall();
    void applyJavaRuntimeProgress(const JavaRuntimeProgress& progress);
    void applyJavaRuntimeResult(const JavaRuntimeResult& result);
    void updateInstallationState(const MinecraftLaunchResult& installation);

    HINSTANCE instance_;
    HWND window_ = nullptr;
    HWND nicknameInput_ = nullptr;
    HFONT inputFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH inputBrush_ = nullptr;
    LauncherViewState viewState_;

    HeaderComponent header_;
    HeroComponent hero_;
    StatusPanelComponent statusPanel_;
    NewsPanelComponent newsPanel_;
    PlayerProfileComponent playerProfile_;
    PlayPanelComponent playPanel_;
    ProgressComponent progress_;

    // O serviço é acessado pela interface genérica para manter a página
    // independente do WinHTTP e do servidor web usado na infraestrutura.
    std::unique_ptr<IFileServerService> fileServerService_;
    std::jthread fileServerThread_;
    std::unique_ptr<IJavaRuntimeService> javaRuntimeService_;
    std::jthread javaRuntimeThread_;
    std::unique_ptr<IMinecraftLauncherService> minecraftLauncherService_;
};
