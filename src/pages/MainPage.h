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
#include "services/EternalAccount.h"
#include "services/GalleryApi.h"
#include "services/MilestoneStore.h"
#include "services/ModInventory.h"
#include "services/NewsFeed.h"
#include "services/PlaytimeApi.h"
#include "services/ScreenshotLibrary.h"
#include "services/PlaytimeStore.h"
#include "services/SkinLookup.h"
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
    float spotlightTarget() const;
    void syncModKnobs();

    // Navigation.
    void showPage(LauncherPage page);
    void scrollContent(int delta, Gdiplus::PointF point);
    void handleModsClick(Gdiplus::PointF point);
    void showPlayMenu();

    // Play flow.
    void handlePrimaryAction();
    void launchVanilla();
    void showGameTrayIcon();
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
    // Closes the installation screen once it ended, giving Home back.
    void closeInstallation();
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
    void startPlaytimeSync(const PlaytimeRecord& local);
    void fetchLeaderboardAsync();
    std::wstring rankingName() const;

    // Avatar: the skin face of the nickname (or Microsoft account) in use.
    std::wstring avatarName() const;
    void scheduleSkinLookup();
    void startSkinLookup();
    const SkinLookupResult* cachedSkin(const std::wstring& name) const;
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

    // News read state: spotlight on Home, badge on the rail, NOVO tags.
    void loadNewsReadState();
    void applyNewsReadState();
    int spotlightIndex() const;
    int unreadNewsCount() const;
    void dismissSpotlight();
    void markNewsRead();

    // Conta Eternal.
    struct EternalReady { EternalAuthResult auth; std::filesystem::path agent; std::wstring error; };
    void openLogin(bool playAfter, std::wstring notice = {});
    void closeLogin();
    void submitLogin();
    void setLoginMode(int mode);
    HWND loginField(int field) const;
    void prepareEternalPlay();
    void signOutEternal();
    float loginTarget() const;
    bool eternalSignedIn() const { return eternalSession_.valid(); }
    std::wstring avatarCacheName() const;
    void startEternalSkinLookup();

    // Skins page.
    struct ExploreSkin
    {
        std::wstring id;      // community skins: the gallery id
        std::wstring title;   // the player (group) or the title it was shared with
        std::wstring author;  // who shared it (community)
        bool group = false;
        bool slim = false;
        std::string png;
        SkinTexture texture;
        bool searched = false;  // found by nickname; `author` then says where
    };
    struct SkinEvent
    {
        enum class Kind { Current, Upload, Reset, ExploreGroup, ExploreCommunity, Share, CommunityRemove, CommunityReport, Search } kind = Kind::Current;
        std::optional<EternalSkin> skin;
        bool success = false;
        std::wstring error;
        // Explorar results.
        std::vector<ExploreSkin> explore;
        bool last = false;                // the loader finished
        std::wstring id;                  // the shared skin acted on
        GalleryUploadResult shared;
        std::wstring source;              // library key of a shared skin
        std::size_t generation = 0;
    };
    void showSkinsTab(bool explore);
    void loadExplore();
    std::size_t exploreCount() const { return (exploreSearch_ ? 1 : 0) + exploreGroup_.size() + exploreCommunity_.size(); }
    void searchSkin();
    const ExploreSkin* exploreSkin(int index) const;
    void selectExplore(int index);
    void saveExploreSkin();
    void shareLibrarySkin();
    void removeCommunitySkin();
    void reportCommunitySkin();
    void handleExploreEvent(SkinEvent& event);
    std::wstring librarySourceKey(std::size_t libraryIndex) const;
    SkinsModel skinsModel(bool preview) const;
    void refreshSkinLibrary();
    void fetchCurrentSkin();
    void selectSkin(int index);
    void addSkin();
    void removeSkin();
    void useSelectedSkin();
    void resetSkin();
    void setSkinsMessage(std::wstring message, bool error = false);
    std::filesystem::path skinLibraryDirectory() const;

    // Modpack changelog.
    void updateModpackChanges();
    void acknowledgeModpackChanges();

    // Play-time levels.
    std::optional<std::uint64_t> serverSecondsForAccount() const;
    void checkMilestone();
    void dismissCelebration();
    float celebrationTarget() const;
    float levelsTarget() const;
    void showLevels(bool open);

    // Screenshots.
    ScreenshotsModel screenshotsModel(bool preview) const;
    void refreshScreenshots();
    void requestScreenshotImages();
    void openScreenshot(int index);
    void closeScreenshot();
    void deleteScreenshot();
    void setScreenshotsMessage(std::wstring message, bool error = false);
    const ScreenshotEntry* currentScreenshot() const;
    std::size_t shotCount() const;

    // Group gallery.
    struct GalleryEvent
    {
        enum class Kind { List, Image, Done, Upload, Remove, Report } kind = Kind::Done;
        GalleryListResult list;
        std::wstring id;
        bool full = false;
        std::unique_ptr<Gdiplus::Bitmap> bitmap;
        GalleryUploadResult upload;
        std::wstring source;
        bool success = false;
        std::wstring error;
    };
    void showGalleryTab(bool group);
    void requestGalleryImages();
    void handleGalleryEvent(GalleryEvent& event);
    const GalleryPhoto* currentGalleryPhoto() const;
    void shareScreenshot();
    void removeGalleryPhoto();
    void reportGalleryPhoto();
    void copyCurrentPicture();

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
    float spotlightReveal_ = 0;
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
    // What still has to be shown on screen. Kept apart from dirty_: a button can repaint
    // synchronously (WM_SETTEXT, EnableWindow) and render the frame before redraw() runs.
    bool invalidateAll_ = true;
    std::vector<RECT> invalid_;
    float playProgressShown_ = -1;
    std::chrono::steady_clock::time_point previousFrame_{};
    LauncherViewState viewState_;

    std::wstring settingsWarning_;
    LauncherSettings settings_;
    SettingsPage settingsPage_;
    bool closing_ = false;
    bool repairOnly_ = false;
    bool vanillaSelected_ = false; // the game picked on the Versions page
    bool vanillaInstalled_ = false;
    bool playingVanilla_ = false;  // the current play request is the unmodified 1.8.9

    ChromeView chrome_;
    HeroComponent hero_;
    HomeView home_;
    VersionsView versionsView_;
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
    NewsReadState newsRead_;

    // Conta Eternal session and its sign-in panel.
    EternalSession eternalSession_;
    std::wstring accountsBaseUrl_;
    HWND loginUser_ = nullptr;
    HWND loginPassword_ = nullptr;
    HWND loginConfirm_ = nullptr;
    int loginMode_ = 0;            // Layout::loginRow modes: sign in, first access, create
    std::wstring loginFirstName_;  // the reserved nickname during a first access
    HFONT loginFont_ = nullptr;
    bool loginOpen_ = false;
    float loginReveal_ = 0;
    bool loginBusy_ = false;
    bool playAfterLogin_ = false;
    std::wstring loginError_;
    std::wstring loginNotice_;
    bool eternalPreparing_ = false;

    // Skins page: index 0 is the account's current skin, then the library files.
    struct SkinEntry { std::wstring name; std::filesystem::path path; std::string png; SkinTexture texture; bool slim = false; };
    SkinsView skinsView_;
    std::vector<SkinEntry> skinLibrary_;
    std::optional<SkinTexture> currentSkin_;
    std::string currentSkinPng_;
    bool currentSkinSlim_ = false;
    bool currentSkinKnown_ = false;
    bool currentSkinLoading_ = false;
    int skinSelected_ = 0;
    std::size_t skinRow_ = 0;
    bool skinSlimChoice_ = false;
    bool skinBusy_ = false;
    std::wstring skinsMessage_;
    bool skinsMessageError_ = false;
    // Explorar: the skins players use on the server, then the ones the community shared.
    bool skinExplore_ = false;
    std::vector<ExploreSkin> exploreGroup_;
    std::vector<ExploreSkin> exploreCommunity_;
    bool exploreLoaded_ = false;
    bool exploreLoading_ = false;
    bool exploreUnavailable_ = false;
    std::size_t exploreGeneration_ = 0;
    int exploreSelected_ = 0;
    std::size_t exploreRow_ = 0;
    bool skinSharing_ = false;
    GallerySharedStore skinsShared_;
    // The skin found by nickname, first in Explorar.
    std::optional<ExploreSkin> exploreSearch_;
    bool skinSearching_ = false;
    HWND skinSearch_ = nullptr;
    std::wstring previewSkinSearch_;

    // Modpack changelog since the list the player last acknowledged.
    ModpackChanges modpackChanges_;
    bool modpackChangesVisible_ = false;
    std::vector<std::wstring> modpackFiles_;

    // Play-time level celebration.
    MilestoneStore milestones_;
    int celebration_ = 0;
    std::uint64_t celebrationSeconds_ = 0;
    std::wstring celebrationName_;
    float celebrationReveal_ = 0;
    // Every level, opened from the Tempo jogado stat.
    bool levelsOpen_ = false;
    float levelsReveal_ = 0;
    std::uint64_t previewLevelsSeconds_ = 0;

    // Screenshots.
    ScreenshotsView screenshotsView_;
    std::vector<ScreenshotEntry> screenshots_;
    std::size_t screenshotRow_ = 0;
    int screenshotViewer_ = -1;
    std::map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>> screenshotThumbnails_;
    std::shared_ptr<Gdiplus::Bitmap> viewerImage_;
    std::wstring viewerImagePath_;
    int viewerWidth_ = 0;
    int viewerHeight_ = 0;
    bool screenshotLoading_ = false;
    std::wstring screenshotsMessage_;
    bool screenshotsMessageError_ = false;

    // Group gallery tab.
    bool galleryTab_ = false;
    std::vector<GalleryPhoto> galleryPhotos_;
    bool galleryLoaded_ = false;
    bool galleryLoading_ = false;
    bool galleryUnavailable_ = false;
    bool galleryWorking_ = false;  // the list/picture worker is running
    bool galleryBusy_ = false;     // an upload, removal or report is running
    std::map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>> galleryThumbnails_;
    std::shared_ptr<Gdiplus::Bitmap> galleryImage_;
    std::wstring galleryImageId_;
    GallerySharedStore galleryShared_;
    std::wstring galleryBaseUrl_;

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

    // Server-side play time and leaderboard. Tests turn the network side off.
    enum class RankingState { Loading, Ready, Unavailable };
    bool playtimeSyncEnabled_ = true;
    bool leaderboardPending_ = false;
    RankingState rankingState_ = RankingState::Loading;
    LeaderboardResult leaderboard_;
    float rankingScroll_ = 0;
    std::optional<std::uint64_t> serverTotalSeconds_;
    // Hours of the preview podium (previews only), chosen to show different levels.
    std::array<std::uint64_t, 3> previewRankingHours_{312, 167, 96};
    int previewExtraPlayers_ = 0;
    int previewTier_ = 0;
    std::wstring previewEternalName_ = L"Steve";
    std::wstring previewLoginUser_;
    float previewRankingScroll_ = 0;
    std::wstring serverTotalName_;

    // Skin faces by lowercase name. Unavailable results are retried on the next lookup.
    std::map<std::wstring, SkinLookupResult> skins_;
    std::wstring skinPending_;

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
    std::jthread leaderboardThread_;
    std::jthread playtimeSyncThread_;
    std::jthread skinThread_;
    std::jthread screenshotThread_;
    std::jthread eternalThread_;
    std::jthread skinActionThread_;
    std::jthread skinFetchThread_;
    std::jthread galleryThread_;
    std::jthread galleryActionThread_;
    std::jthread exploreThread_;
    std::jthread exploreActionThread_;
    std::jthread skinSearchThread_;
    // Last member: destruction joins before any state captured by the worker is destroyed.
    std::jthread launcherUpdateThread_;
    // Last: all captured services, tracking state and HWND outlive this worker.
    std::jthread installationThread_;
};
