#pragma once

#include "pages/SettingsPage.h"
#include "components/HeaderComponent.h"
#include "components/HeroComponent.h"
#include "components/NewsPanelComponent.h"
#include "components/PlayPanelComponent.h"
#include "components/PlayerProfileComponent.h"
#include "components/ProgressComponent.h"
#include "components/StatusPanelComponent.h"
#include "components/InstallationPanelComponent.h"
#include "services/InstallationCoordinator.h"
#include "services/IFileServerService.h"
#include "services/IJavaRuntimeService.h"
#include "services/IGameInstallationService.h"
#include "services/IMinecraftLauncherService.h"
#include "services/MicrosoftAuthService.h"
#include "services/LauncherUpdateService.h"
#include "types/LauncherState.h"

#include <windows.h>

#include <memory>
#include <thread>
#include <array>
#include <filesystem>

class MainPage
{
public:
    explicit MainPage(HINSTANCE instance);
    ~MainPage();

    bool create(int showCommand, bool checkLauncherUpdate = true, std::wstring updateWarning = {});
    bool renderPreviews(const std::filesystem::path& directory);
    bool handleKeyboard(MSG& message);

private:
    friend class WindowPaintingTest;
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void paint();
    bool prepareFrame();
    void render(Gdiplus::Graphics& graphics, bool preview = false, bool restingSettings = false);
    bool renderSettingsTransition(Gdiplus::Graphics& graphics);
    void createActionControls();
    void layoutControls();
    void syncControls();
    void redraw(bool animationFrame = false);
    void animate();
    float pageOffset() const;
    void positionPageControls();
    void updateMotionPreference();
    static LRESULT CALLBACK controlProcedure(HWND window, UINT message, WPARAM wParam,
                                             LPARAM lParam, UINT_PTR id, DWORD_PTR data);
    void enableDarkTitleBar() const;
    void createNicknameInput();
    void layoutNicknameInput() const;
    void handlePrimaryAction();
    void beginPlay();
    void startMicrosoftSignIn();
    void pollGameProcess();
    void showSettings();
    void hideSettings();
    void saveSettings();
    void checkFileServerAsync();
    void checkLauncherUpdateAsync(bool confirmed = false);
    void applyLauncherUpdateProgress(const LauncherUpdateProgress& progress);
    void applyFileServerResult(const FileServerManifestResponse& response);
    void cancelInstallation();
    void consumeInstallationProgress();
    void openInstallationLog();
    void launchPreparedMinecraft(const InstallationResult& result);
    void updateInstallationState(const MinecraftLaunchResult& installation);

    HINSTANCE instance_;
    HWND window_ = nullptr;
    HWND nicknameInput_ = nullptr;
    HWND updateDownloadButton_ = nullptr;
    HWND cancelInstallationButton_ = nullptr;
    HWND installationLogButton_ = nullptr;
    HFONT inputFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH inputBrush_ = nullptr;
    std::array<HWND, 4> actions_{};
    std::array<float, 4> hover_{};
    std::array<bool, 4> hovered_{};
    std::array<bool,2> installationHovered_{};
    std::array<float,2> installationHover_{};
    float identitySelection_ = 0;
    float portalEntrance_ = 1;
    bool portalHovered_ = false;
    float portalEnergy_ = 0;
    float portalBurst_ = 0;
    float playPress_ = 0;
    float playPulse_ = 0;
    float playSweep_ = 1;
    float playLight_ = .5F;
    float playLightTarget_ = .5F;
    float pageTravel_ = 1;
    int positionedScene_ = -1;
    int positionedOffset_ = 0;
    std::array<RECT,7> mainControlBounds_{};
    float particlePhase_ = 0;
    bool ambientMotion_ = true;
    std::array<float,6> completionPulse_{};
    std::array<InstallationStepState,6> previousSteps_{};
    std::uint64_t pulseAttempt_ = 0;
    float installationFill_ = 0;
    std::uint64_t fillAttempt_ = 0;
    std::uint64_t fillTransfer_ = 0;
    InstallationStage fillStage_ = InstallationStage::Java;
    std::unique_ptr<Gdiplus::Bitmap> leftTransition_;
    Gdiplus::RectF transitionBounds_;
    float transitionTime_ = 1;
    int previousScene_ = 0;
    float displayedProgress_ = 0;
    float animationPhase_ = 0;
    float viewScale_ = 1;
    float offsetX_ = 0;
    float offsetY_ = 0;
    bool motionEnabled_ = true;
    bool animationTimer_ = false;
    std::unique_ptr<Gdiplus::Bitmap> frame_;
    std::unique_ptr<Gdiplus::Bitmap> settingsTransitionFrame_;
    bool frameDirty_ = true;
    std::chrono::steady_clock::time_point previousFrame_{};
    LauncherViewState viewState_;

    std::wstring settingsWarning_;
    LauncherSettings settings_;
    SettingsPage settingsPage_;
    bool settingsVisible_ = false;
    bool minimizedForGame_ = false;
    bool closing_ = false;

    HeaderComponent header_;
    HeroComponent hero_;
    StatusPanelComponent statusPanel_;
    NewsPanelComponent newsPanel_;
    PlayerProfileComponent playerProfile_;
    PlayPanelComponent playPanel_;
    ProgressComponent progress_;
    InstallationPanelComponent installationPanel_;
    InstallationTrackingState installation_;
    InstallationMailbox installationMailbox_;

    // The service is accessed through the generic interface to keep the page
    // independent of WinHTTP and of the web server used in the infrastructure.
    std::unique_ptr<IFileServerService> fileServerService_;
    std::jthread fileServerThread_;
    std::unique_ptr<IJavaRuntimeService> javaRuntimeService_;
    std::unique_ptr<IGameInstallationService> gameInstallationService_;
    std::jthread microsoftAuthThread_;
    std::unique_ptr<IMinecraftLauncherService> minecraftLauncherService_;
    std::wstring fileServerManifest_;
    std::wstring pendingNickname_;
    MinecraftIdentity microsoftIdentity_;
    MinecraftIdentity pendingIdentity_;
    bool originalMode_ = false;
    bool refreshBeforePlay_ = false;
    bool updatingLauncher_ = false;
    std::wstring updateWarning_;
    // Last member: destruction joins before any state captured by the worker is destroyed.
    std::jthread launcherUpdateThread_;
    // Last: all captured services, tracking state and HWND outlive this worker.
    std::jthread installationThread_;
};
