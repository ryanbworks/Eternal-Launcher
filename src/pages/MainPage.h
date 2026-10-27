#pragma once

#include "app/TrayIcon.h"
#include "components/Buttons.h"
#include "components/ChromeView.h"
#include "components/ContentViews.h"
#include "components/HeroComponent.h"
#include "components/HomeView.h"
#include "components/InstallationPanelComponent.h"
#include "pages/SettingsPage.h"
#include "services/IFileServerService.h"
#include "services/IGameInstallationService.h"
#include "services/IJavaRuntimeService.h"
#include "services/IMinecraftLauncherService.h"
#include "services/InstallationCoordinator.h"
#include "services/LauncherUpdateService.h"
#include "services/MicrosoftAuthService.h"
#include "services/ModInventory.h"
#include "services/PlaytimeStore.h"
#include "types/LauncherState.h"

#include <windows.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

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

    // One owner-drawn native button. Its pixels come from the pre-rendered frame.
    struct Control
    {
        int id = 0;
        HWND window = nullptr;
        bool chrome = false;
        bool hovered = false;
        float hover = 0;
        RECT rect{};  // client pixels, refreshed on layout
    };

    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK controlProcedure(HWND window, UINT message, WPARAM wParam,
                                             LPARAM lParam, UINT_PTR id, DWORD_PTR data);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT hitTest(LPARAM lParam) const;
    void handleCommand(int id, int notification);

    // Drawing.
    void paint();
    bool prepareFrame();
    void render(Gdiplus::Graphics& graphics, float width, float height, bool preview = false);
    ControlVisual visual(int id, bool preview) const;
    HomeModel homeModel(bool preview) const;
    ModsModel modsModel() const;
    void redraw(bool animationFrame = false);
    void releaseFrame();
    void markRect(RECT rect);
    void markContent(const Gdiplus::RectF& bounds, float inflate = 0);
    void markChrome(const Gdiplus::RectF& bounds, float inflate = 0);
    void markControl(int id);
    float playProgressTarget() const;
    void animate();
    void updateMotionPreference();

    // Controls.
    void createControls();
    void createNicknameInput();
    void layoutControls();
    void syncControls();
    Control* findControl(int id);
    const Control* findControl(int id) const;
    Gdiplus::RectF controlBounds(int id) const;
    bool controlVisible(int id) const;
    bool controlEnabled(int id) const;
    std::wstring controlLabel(int id) const;
    bool chromeControl(int id) const;
    [[nodiscard]] Gdiplus::PointF toContent(int x, int y) const;
    float chromeWidth() const;
    float chromeHeight() const;
    float navigationTarget() const;
    float toastTarget() const;
    void syncModKnobs();

    // Navigation.
    void showPage(LauncherPage page);
    void scrollContent(int delta, Gdiplus::PointF point);
    void handleModsClick(Gdiplus::PointF point);
    void showPlayMenu();

    // Play flow.
    void handlePrimaryAction();
    void beginPlay(bool repairOnly = false);
    // Plays offline when the file server is down and the installation is complete,
    // otherwise prepares the game (or retries the server first).
    void startGame();
    void retryServer();
    bool offlineReady() const;
    void startRepair();
    void startMicrosoftSignIn();
    void pollGameProcess();
    void saveSettings();
    void checkFileServerAsync();
    void fetchNewsAsync();
    void checkLauncherUpdateAsync(bool confirmed = false);
    void applyLauncherUpdateProgress(const LauncherUpdateProgress& progress);
    void applyFileServerResult(const FileServerManifestResponse& response);
    void cancelInstallation();
    void consumeInstallationProgress();
    void launchPreparedMinecraft(const InstallationResult& result, bool offline = false);
    void showOfflineState(std::wstring message);
    void updateInstallationState(const MinecraftLaunchResult& installation);
    bool gameRunning() const;
    bool canRepair() const;
    std::wstring nicknameText() const;

    // Play time and the notification-area icon.
    void startPlaySession();
    void flushPlaytime();
    void finishPlaySession();
    void updateTrayTooltip();
    void hideToTray(bool closedByUser);
    void restoreFromTray();
    void showTrayMenu(POINT anchor);
    void shutdown();

    // Files, folders and support.
    std::filesystem::path gameDirectory() const;
    void refreshMods();
    void addMod();
    bool openPath(const std::filesystem::path& path, bool createDirectory = false);
    void openLog(const std::filesystem::path& file);
    void copySupportInformation();
    void setHelpMessage(std::wstring message, bool error = false);
    void checkVersionToast();

    HINSTANCE instance_;
    HWND window_ = nullptr;
    HWND nicknameInput_ = nullptr;
    HFONT inputFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH surfaceBrush_ = nullptr;
    HBRUSH inputBrush_ = nullptr;
    HBRUSH sunkenBrush_ = nullptr;
    std::vector<Control> controls_;

    LauncherPage page_ = LauncherPage::Home;
    float pageFade_ = 1;
    float time_ = 0;              // seconds; drives idle effects (breathing, blinking, sheens)
    float navIndicator_ = 60;     // logical top of the sliding navigation selection
    float toastReveal_ = 0;
    float toastAge_ = 0;          // milliseconds the toast has been on screen
    float playSweep_ = 1;
    float playPulse_ = 0;
    float playPress_ = 0;
    float playLight_ = .5F;
    float playLightTarget_ = .5F;
    std::map<std::wstring, float> modKnobs_;
    bool windowActive_ = true;
    float identitySelection_ = 0;
    float portalEntrance_ = 1;
    bool portalHovered_ = false;
    float portalEnergy_ = 0;
    float portalBurst_ = 0;
    float particlePhase_ = 0;
    bool ambientMotion_ = true;
    std::array<float, 6> completionPulse_{};
    std::array<InstallationStepState, 6> previousSteps_{};
    std::uint64_t pulseAttempt_ = 0;
    float installationFill_ = 0;
    std::uint64_t fillAttempt_ = 0;
    std::uint64_t fillTransfer_ = 0;
    InstallationStage fillStage_ = InstallationStage::Java;
    float displayedProgress_ = 0;
    float animationPhase_ = 0;
    float viewScale_ = 1;
    float offsetX_ = 0;
    float offsetY_ = 0;
    float clientWidth_ = 0;
    float clientHeight_ = 0;
    bool motionEnabled_ = true;
    bool animationTimer_ = false;
    // The frame is a DIB section wrapped by GDI+: drawing uses GDI+, presenting the
    // window and every owner-drawn child is a plain BitBlt from frameDc_.
    std::unique_ptr<Gdiplus::Bitmap> frame_;
    HDC frameDc_ = nullptr;
    HBITMAP frameBitmap_ = nullptr;
    HGDIOBJ frameOldBitmap_ = nullptr;
    // frameDirty_ re-renders everything; otherwise only `dirty_` (client pixels) is redrawn.
    bool frameDirty_ = true;
    std::vector<RECT> dirty_;
    float playProgressShown_ = -1;
    std::chrono::steady_clock::time_point previousFrame_{};
    LauncherViewState viewState_;

    std::wstring settingsWarning_;
    LauncherSettings settings_;
    SettingsPage settingsPage_;
    bool closing_ = false;
    bool repairOnly_ = false;

    ChromeView chrome_;
    HeroComponent hero_;
    HomeView home_;
    NewsView newsView_;
    ModsView modsView_;
    HelpView helpView_;
    InstallationPanelComponent installationPanel_;
    InstallationTrackingState installation_;
    InstallationMailbox installationMailbox_;

    // News.
    std::size_t newsFirst_ = 0;
    bool newsLoading_ = false;
    bool newsRequested_ = false;
    std::wstring newsError_;
    bool toastVisible_ = false;
    std::wstring toastText_;

    // Mods.
    ModInventorySnapshot mods_;
    bool modsScanned_ = false;
    float serverScroll_ = 0;
    float userScroll_ = 0;
    int modsHoverRow_ = -1;
    int modsHoverAction_ = -1;
    std::wstring modsMessage_;
    bool modsMessageError_ = false;

    // Help.
    std::wstring helpMessage_;
    bool helpMessageError_ = false;

    // Play time and tray.
    PlaySession playSession_;
    PlaytimeRecord playtime_;
    TrayIcon trayIcon_;
    bool hiddenToTray_ = false;
    bool exitAfterGame_ = false;
    bool forceExit_ = false;
    bool trayNoticeShown_ = false;

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
    bool serverOffline_ = false;
    bool fileServerPending_ = false;
    bool updatingLauncher_ = false;
    std::wstring updateWarning_;
    std::jthread newsThread_;
    // Last member: destruction joins before any state captured by the worker is destroyed.
    std::jthread launcherUpdateThread_;
    // Last: all captured services, tracking state and HWND outlive this worker.
    std::jthread installationThread_;
};
