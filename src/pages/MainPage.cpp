#include "pages/MainPage.h"

#include "components/Layout.h"
#include "config/LauncherConfig.h"
#include "config/NicknameStore.h"
#include "constants/LauncherConstants.h"
#include "mocks/MockLauncherData.h"
#include "services/GameInstallationService.h"
#include "services/HttpFileService.h"
#include "services/JavaRuntimeService.h"
#include "services/MinecraftLauncherService.h"
#include "services/NewsFeed.h"
#include "services/VanillaMinecraftService.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    using namespace LauncherConstants;

    // Minecraft names are ASCII and case-insensitive; this is the skin cache key.
    std::wstring skinKey(std::wstring name)
    {
        for (auto& c : name) if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
        return name;
    }

    // Creation order is also the keyboard tab order.
    struct ControlSpec { int id; bool chrome; bool tabStop; const wchar_t* label; };
    constexpr ControlSpec ControlSpecs[] = {
        {NavHomeId, true, true, L"Início"},
        {NavVersionsId, true, true, L"Versões"},
        {NavNewsId, true, true, L"Notícias"},
        {NavModsId, true, true, L"Mods"},
        {NavScreenshotsId, true, true, L"Capturas"},
        {NavSkinsId, true, true, L"Skins"},
        {NavSettingsId, true, true, L"Configurações"},
        {NavHelpId, true, true, L"Ajuda"},
        {LoginUserInputId, false, true, nullptr},
        {LoginPasswordInputId, false, true, nullptr},
        {LoginConfirmInputId, false, true, nullptr},
        {LoginSubmitButtonId, false, true, L"Entrar na Conta Eternal"},
        {LoginRegisterButtonId, false, true, L"Criar conta no site"},
        {LoginCloseButtonId, false, true, L"Fechar"},
        {SpotlightReadButtonId, false, true, L"Ler novidade"},
        {SpotlightDismissButtonId, false, true, L"Ver depois"},
        {CelebrationButtonId, false, true, L"Fechar conquista"},
        {ModpackChangesModsButtonId, false, true, L"Ver mods"},
        {ModpackChangesOkButtonId, false, true, L"Entendi as mudanças do modpack"},
        {NicknameInputId, false, true, nullptr},
        {EditNicknameButtonId, false, true, L"Editar nickname"},
        {AccountSignOutButtonId, false, true, L"Sair da Conta Eternal"},
        {OfflineButtonId, false, true, L"Nickname"},
        {OriginalButtonId, false, true, L"Conta original"},
        {LevelsButtonId, false, true, L"Ver todos os níveis"},
        {LevelsCloseButtonId, false, true, L"Fechar níveis"},
        {VersionEternalButtonId, false, true, L"Modpack Eternal"},
        {VersionVanillaButtonId, false, true, L"Minecraft 1.8.9"},
        {VersionChangeButtonId, false, true, L"Trocar versão"},
        {UpdateDownloadButtonId, false, true, L"Baixar launcher"},
        {RetryServerButtonId, false, true, L"Tentar conectar ao servidor"},
        {PlayButtonId, false, true, L"Jogar"},
        {PlayMenuButtonId, false, true, L"Mais ações"},
        {QuickFolderButtonId, false, true, L"Pasta do jogo"},
        {QuickRepairButtonId, false, true, L"Reparar"},
        {QuickLogsButtonId, false, true, L"Logs"},
        {NewsCardFirstId, false, true, L"Notícia 1"},
        {NewsCardFirstId + 1, false, true, L"Notícia 2"},
        {NewsAllButtonId, false, true, L"Ver todas as notícias"},
        {InstallationLogButtonId, false, true, L"Abrir log"},
        {CancelInstallationButtonId, false, true, L"Cancelar instalação"},
        {InstallationRetryButtonId, false, true, L"Tentar de novo"},
        {InstallationCloseButtonId, false, true, L"Fechar"},
        {NewsLinkFirstId, false, true, L"Abrir notícia 1"},
        {NewsLinkFirstId + 1, false, true, L"Abrir notícia 2"},
        {NewsLinkFirstId + 2, false, true, L"Abrir notícia 3"},
        {NewsLinkFirstId + 3, false, true, L"Abrir notícia 4"},
        {ModsRefreshButtonId, false, true, L"Atualizar lista"},
        {ModsFolderButtonId, false, true, L"Abrir pasta"},
        {ModsAddButtonId, false, true, L"Adicionar mod"},
        {SkinMineTabId, false, true, L"Minhas skins"},
        {SkinExploreTabId, false, true, L"Explorar skins"},
        {SkinSearchInputId, false, true, nullptr},
        {SkinSearchButtonId, false, true, L"Buscar skin pelo nick"},
        {SkinFolderButtonId, false, true, L"Abrir pasta de skins"},
        {SkinAddButtonId, false, true, L"Adicionar skin"},
        {SkinRefreshButtonId, false, true, L"Atualizar skins"},
        {SkinShareButtonId, false, true, L"Compartilhar skin com o grupo"},
        {SkinRemoveButtonId, false, true, L"Remover skin da biblioteca"},
        {SkinReportButtonId, false, true, L"Denunciar skin"},
        {SkinClassicButtonId, false, true, L"Braços clássicos"},
        {SkinSlimButtonId, false, true, L"Braços finos"},
        {SkinUseButtonId, false, true, L"Usar esta skin"},
        {SkinResetButtonId, false, true, L"Voltar para a skin padrão"},
        {SkinSaveButtonId, false, true, L"Salvar na biblioteca"},
        {SkinSignInButtonId, false, true, L"Entrar na Conta Eternal"},
        {SkinCardFirstId, false, true, L"Skin 1"}, {SkinCardFirstId + 1, false, true, L"Skin 2"},
        {SkinCardFirstId + 2, false, true, L"Skin 3"}, {SkinCardFirstId + 3, false, true, L"Skin 4"},
        {SkinCardFirstId + 4, false, true, L"Skin 5"}, {SkinCardFirstId + 5, false, true, L"Skin 6"},
        {SkinCardFirstId + 6, false, true, L"Skin 7"}, {SkinCardFirstId + 7, false, true, L"Skin 8"},
        {SkinCardFirstId + 8, false, true, L"Skin 9"}, {SkinCardFirstId + 9, false, true, L"Skin 10"},
        {SkinCardFirstId + 10, false, true, L"Skin 11"}, {SkinCardFirstId + 11, false, true, L"Skin 12"},
        {ScreenshotsMineTabId, false, true, L"Minhas capturas"},
        {ScreenshotsGroupTabId, false, true, L"Galeria do grupo"},
        {ScreenshotsRefreshButtonId, false, true, L"Atualizar capturas"},
        {ScreenshotsFolderButtonId, false, true, L"Abrir pasta de capturas"},
        {ScreenshotCardFirstId, false, true, L"Captura 1"},
        {ScreenshotCardFirstId + 1, false, true, L"Captura 2"},
        {ScreenshotCardFirstId + 2, false, true, L"Captura 3"},
        {ScreenshotCardFirstId + 3, false, true, L"Captura 4"},
        {ScreenshotCardFirstId + 4, false, true, L"Captura 5"},
        {ScreenshotCardFirstId + 5, false, true, L"Captura 6"},
        {ScreenshotCardFirstId + 6, false, true, L"Captura 7"},
        {ScreenshotCardFirstId + 7, false, true, L"Captura 8"},
        {ScreenshotCardFirstId + 8, false, true, L"Captura 9"},
        {ScreenshotCardFirstId + 9, false, true, L"Captura 10"},
        {ScreenshotCardFirstId + 10, false, true, L"Captura 11"},
        {ScreenshotCardFirstId + 11, false, true, L"Captura 12"},
        {ViewerBackButtonId, false, true, L"Voltar para as capturas"},
        {ViewerShareButtonId, false, true, L"Compartilhar com o grupo"},
        {ViewerCopyButtonId, false, true, L"Copiar captura"},
        {ViewerFolderButtonId, false, true, L"Mostrar na pasta"},
        {ViewerOpenButtonId, false, true, L"Abrir no visualizador do Windows"},
        {ViewerDeleteButtonId, false, true, L"Mover captura para a Lixeira"},
        {ViewerReportButtonId, false, true, L"Denunciar captura"},
        {ViewerPreviousButtonId, false, true, L"Captura anterior"},
        {ViewerNextButtonId, false, true, L"Próxima captura"},
        {HelpRepairButtonId, false, true, L"Reparar instalação"},
        {HelpInstallLogButtonId, false, true, L"Log da instalação"},
        {HelpForgeLogButtonId, false, true, L"Log do Forge"},
        {HelpGameLogButtonId, false, true, L"Log do jogo"},
        {HelpLogsFolderButtonId, false, true, L"Pasta de logs"},
        {HelpCopySupportButtonId, false, true, L"Copiar informações"},
        {HelpReportIssueButtonId, false, true, L"Relatar no GitHub"},
        {HelpDataFolderButtonId, false, true, L"Abrir pasta de dados"},
        {MinimizeButtonId, true, false, L"Minimizar"},
        {MaximizeButtonId, true, false, L"Maximizar"},
        {CloseButtonId, true, false, L"Fechar"},
    };

    constexpr wchar_t IssueUrl[] = L"https://github.com/ryanbworks/native-game-launcher/issues/new";

    std::int64_t wallClock() { return static_cast<std::int64_t>(std::time(nullptr)); }

    bool inRange(int id, int first, int count) { return id >= first && id < first + count; }

    std::wstring utf8ToWide(std::string_view text)
    {
        if (text.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring result(static_cast<std::size_t>(std::max(length, 0)), L'\0');
        if (length > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }

    std::wstring registryText(const wchar_t* name)
    {
        std::array<wchar_t, 128> buffer{};
        DWORD size = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
        if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", name, RRF_RT_REG_SZ,
                nullptr, buffer.data(), &size) != ERROR_SUCCESS) return {};
        return buffer.data();
    }

    // Hides the Windows account name when the player shares support information.
    std::wstring anonymizePath(std::wstring path)
    {
        std::array<wchar_t, MAX_PATH> profile{};
        const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", profile.data(), static_cast<DWORD>(profile.size()));
        if (length == 0 || length >= profile.size()) return path;
        const std::wstring prefix(profile.data(), length);
        if (path.size() >= prefix.size() && CompareStringOrdinal(path.c_str(), static_cast<int>(prefix.size()),
                prefix.c_str(), static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL)
            path = L"%USERPROFILE%" + path.substr(prefix.size());
        return path;
    }

    int frameThickness(HWND window, bool vertical)
    {
        const UINT dpi = window ? GetDpiForWindow(window) : 96;
        return GetSystemMetricsForDpi(vertical ? SM_CYFRAME : SM_CXFRAME, dpi) +
               GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    }
}

MainPage::MainPage(HINSTANCE instance)
    : instance_(instance),
      backgroundBrush_(CreateSolidBrush(RGB(13, 14, 20))),
      surfaceBrush_(CreateSolidBrush(RGB(22, 23, 33))),
      inputBrush_(CreateSolidBrush(RGB(30, 31, 44))),
      sunkenBrush_(CreateSolidBrush(RGB(27, 28, 40))),
      viewState_(MockLauncherData::createInitialState()),
      settings_(LauncherSettingsStore::load(LauncherSettingsStore::dataDirectory(), &settingsWarning_)),
      fileServerService_(std::make_unique<HttpFileService>(
          std::wstring(LauncherConfig::FileServerBaseUrl))),
      javaRuntimeService_(std::make_unique<JavaRuntimeService>()),
      gameInstallationService_(std::make_unique<GameInstallationService>(settings_)),
      minecraftLauncherService_(std::make_unique<MinecraftLauncherService>(settings_))
{
    viewState_.news = {NewsFeed::releaseNotes()};
    playtime_ = PlaytimeStore::load(LauncherSettingsStore::dataDirectory());
}

MainPage::~MainPage()
{
    installationThread_.request_stop();
    fileServerThread_.request_stop();
    microsoftAuthThread_.request_stop();
    newsThread_.request_stop();
    if (installationThread_.joinable()) installationThread_.join();
    if (fileServerThread_.joinable()) fileServerThread_.join();
    if (microsoftAuthThread_.joinable()) microsoftAuthThread_.join();
    if (newsThread_.joinable()) newsThread_.join();
    leaderboardThread_.request_stop();
    playtimeSyncThread_.request_stop();
    if (leaderboardThread_.joinable()) leaderboardThread_.join();
    if (playtimeSyncThread_.joinable()) playtimeSyncThread_.join();
    launcherUpdateThread_.request_stop();
    if (launcherUpdateThread_.joinable()) launcherUpdateThread_.join();
    eternalThread_.request_stop();
    if (eternalThread_.joinable()) eternalThread_.join();
    skinActionThread_.request_stop();
    skinFetchThread_.request_stop();
    exploreThread_.request_stop();
    exploreActionThread_.request_stop();
    skinSearchThread_.request_stop();
    if (skinSearchThread_.joinable()) skinSearchThread_.join();
    if (skinActionThread_.joinable()) skinActionThread_.join();
    if (skinFetchThread_.joinable()) skinFetchThread_.join();
    if (exploreThread_.joinable()) exploreThread_.join();
    if (exploreActionThread_.joinable()) exploreActionThread_.join();
    for (MSG pending{}; PeekMessageW(&pending, nullptr, SkinEventMessage, SkinEventMessage, PM_REMOVE);)
        delete reinterpret_cast<SkinEvent*>(pending.lParam);
    for (MSG pending{}; PeekMessageW(&pending, nullptr, EternalAuthMessage, EternalAuthMessage, PM_REMOVE);)
        delete reinterpret_cast<EternalAuthResult*>(pending.lParam);
    for (MSG pending{}; PeekMessageW(&pending, nullptr, EternalReadyMessage, EternalReadyMessage, PM_REMOVE);)
        delete reinterpret_cast<EternalReady*>(pending.lParam);
    for (MSG pending{}; PeekMessageW(&pending, nullptr, EternalFirstAccessMessage, EternalFirstAccessMessage, PM_REMOVE);)
        delete reinterpret_cast<EternalAccount::FirstAccess*>(pending.lParam);
    if (loginFont_) DeleteObject(loginFont_);
    screenshotThread_.request_stop();
    galleryThread_.request_stop();
    galleryActionThread_.request_stop();
    if (screenshotThread_.joinable()) screenshotThread_.join();
    if (galleryThread_.joinable()) galleryThread_.join();
    if (galleryActionThread_.joinable()) galleryActionThread_.join();
    // Results the workers posted after the window closed are never read; nothing else owns them.
    for (MSG pending{}; PeekMessageW(&pending, nullptr, ScreenshotImageMessage, ScreenshotImageMessage, PM_REMOVE);)
        delete reinterpret_cast<ScreenshotImage*>(pending.lParam);
    for (MSG pending{}; PeekMessageW(&pending, nullptr, GalleryEventMessage, GalleryEventMessage, PM_REMOVE);)
        delete reinterpret_cast<GalleryEvent*>(pending.lParam);
    releaseFrame();
    for (auto* brush : {backgroundBrush_, surfaceBrush_, inputBrush_, sunkenBrush_})
        if (brush) DeleteObject(brush);
    if (inputFont_) DeleteObject(inputFont_);
}

bool MainPage::create(int showCommand, bool checkLauncherUpdate, std::wstring updateWarning)
{
    updateWarning_ = std::move(updateWarning);
    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = MainPage::windowProcedure;
    windowClass.lpszClassName = WindowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = backgroundBrush_;
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.hIcon = LoadIcon(instance_, MAKEINTRESOURCE(101));
    windowClass.hIconSm = windowClass.hIcon;
    if (RegisterClassEx(&windowClass) == 0) return false;

    // The caption is drawn by the launcher; WS_OVERLAPPEDWINDOW keeps snapping,
    // the shadow and the minimize/maximize animations.
    constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    // Include native children in the final buffered presentation, not just the GDI+ canvas.
    constexpr DWORD extendedStyle = WS_EX_COMPOSITED;

    // Center on the work area of the monitor under the cursor, without covering the taskbar.
    POINT cursor{};
    GetCursorPos(&cursor);
    MONITORINFO monitorInfo{sizeof(MONITORINFO)};
    const auto monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfo(monitor, &monitorInfo)) return false;
    const RECT& workArea = monitorInfo.rcWork;
    const UINT dpi = GetDpiForSystem();
    const float scale = std::min({static_cast<float>(dpi) / 96.0F,
        static_cast<float>(workArea.right - workArea.left - 48) / WindowWidth,
        static_cast<float>(workArea.bottom - workArea.top - 48) / WindowHeight});
    const int width = static_cast<int>(WindowWidth * scale);
    const int height = static_cast<int>(WindowHeight * scale);
    const int x = std::max(workArea.left, workArea.left + ((workArea.right - workArea.left) - width) / 2);
    const int y = std::max(workArea.top, workArea.top + ((workArea.bottom - workArea.top) - height) / 2);

    window_ = CreateWindowEx(extendedStyle, WindowClassName, WindowTitle, windowStyle,
        x, y, width, height, nullptr, nullptr, instance_, this);
    if (window_ == nullptr) return false;

    BOOL dark = TRUE;
    DwmSetWindowAttribute(window_, 20, &dark, sizeof(dark));            // DWMWA_USE_IMMERSIVE_DARK_MODE
    const int corners = 2;                                              // DWMWCP_ROUND
    DwmSetWindowAttribute(window_, 33, &corners, sizeof(corners));      // DWMWA_WINDOW_CORNER_PREFERENCE
    const COLORREF border = RGB(42, 43, 58);
    DwmSetWindowAttribute(window_, 34, &border, sizeof(border));        // DWMWA_BORDER_COLOR
    const MARGINS shadow{0, 0, 0, 1};
    DwmExtendFrameIntoClientArea(window_, &shadow);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    updateMotionPreference();
    createControls();
    if (!settingsPage_.create(window_, instance_)) { DestroyWindow(window_); return false; }
    settingsPage_.show(false);
    layoutControls();
    loadNewsReadState();
    milestones_.load(LauncherSettingsStore::dataDirectory());
    galleryShared_.load(LauncherSettingsStore::dataDirectory());
    skinsShared_.load(LauncherSettingsStore::dataDirectory(), L"skins-shared.txt");
    galleryBaseUrl_ = GalleryApi::baseUrl();
    accountsBaseUrl_ = EternalAccount::baseUrl();
    if (auto session = EternalSessionStore::load(LauncherSettingsStore::dataDirectory())) {
        eternalSession_ = std::move(*session);
        startSkinLookup();
    }
    syncControls();
    portalEntrance_ = motionEnabled_ ? 0.0F : 1.0F;
    portalBurst_ = motionEnabled_ ? 1.0F : 0.0F;
    ShowWindow(window_, showCommand);
    SendMessageW(window_, WM_CHANGEUISTATE, MAKEWPARAM(UIS_INITIALIZE, 0), 0);
    redraw();
    UpdateWindow(window_);
    if (checkLauncherUpdate) checkLauncherUpdateAsync();
    else checkFileServerAsync();
    return true;
}

LRESULT CALLBACK MainPage::windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainPage* page = nullptr;
    if (message == WM_NCCREATE) {
        const auto createData = reinterpret_cast<CREATESTRUCT*>(lParam);
        page = static_cast<MainPage*>(createData->lpCreateParams);
        page->window_ = window;
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(page));
    } else {
        page = reinterpret_cast<MainPage*>(GetWindowLongPtr(window, GWLP_USERDATA));
    }
    return page != nullptr ? page->handleMessage(message, wParam, lParam) : DefWindowProc(window, message, wParam, lParam);
}

LRESULT MainPage::hitTest(LPARAM lParam) const
{
    POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(window_, &point);
    RECT client{};
    GetClientRect(window_, &client);
    if (!IsZoomed(window_)) {
        const int frameX = frameThickness(window_, false);
        const int frameY = frameThickness(window_, true);
        const bool left = point.x < frameX, right = point.x >= client.right - frameX;
        const bool top = point.y < frameY, bottom = point.y >= client.bottom - frameY;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
    }
    // Children (caption and navigation buttons) receive their own hits first.
    if (static_cast<float>(point.y) < TitleBarHeight * viewScale_) return HTCAPTION;
    return HTCLIENT;
}

LRESULT MainPage::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCCALCSIZE:
        if (wParam && IsZoomed(window_)) {
            // A maximized window extends past the monitor by its frame; keep the content on screen.
            auto& rect = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0];
            const int frameX = frameThickness(window_, false), frameY = frameThickness(window_, true);
            rect.left += frameX; rect.right -= frameX; rect.top += frameY; rect.bottom -= frameY;
        }
        return 0;
    case WM_NCHITTEST:
        return hitTest(lParam);
    case WM_NCACTIVATE:
        windowActive_ = wParam != FALSE;
        redraw();
        // -1 keeps DefWindowProc from painting a native frame over the custom title bar.
        return DefWindowProc(window_, message, wParam, -1);
    case WM_PAINT:
        paint();
        return 0;
    case WM_PRINTCLIENT:
        if (prepareFrame())
            BitBlt(reinterpret_cast<HDC>(wParam), 0, 0, static_cast<int>(frame_->GetWidth()), static_cast<int>(frame_->GetHeight()),
                frameDc_, 0, 0, SRCCOPY);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case InterfaceRedrawMessage:
        redraw();
        return 0;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) layoutControls();
        redraw();
        return 0;
    case WM_SHOWWINDOW:
        if (!wParam) portalHovered_ = false;
        // Visibility is applied after this message; defer scheduling until that state is observable.
        PostMessage(window_, InterfaceRedrawMessage, 0, 0);
        return 0;
    case WM_ACTIVATEAPP:
        PostMessage(window_, InterfaceRedrawMessage, 0, 0);
        return 0;
    case WM_MOUSEMOVE:
    {
        const auto point = toContent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        bool changed = false;
        const bool hovered = page_ == LauncherPage::Home && !installation_.visible && !loginOpen_ && !levelsOpen_ &&
            Layout::Hero.Contains(point.X, point.Y);
        if (hovered != portalHovered_) { portalHovered_ = hovered; changed = true; }
        if (page_ == LauncherPage::Mods) {
            const auto hit = ModsView::hitTest(modsModel(), point.X, point.Y);
            const int action = hit.action == ModsView::Action::Toggle ? 0 : hit.action == ModsView::Action::Remove ? 1 : -1;
            if (hit.row != modsHoverRow_ || action != modsHoverAction_) {
                modsHoverRow_ = hit.row; modsHoverAction_ = action; changed = true;
            }
        }
        if (changed) {
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window_, 0};
            TrackMouseEvent(&track);
            redraw();
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        portalHovered_ = false;
        modsHoverRow_ = modsHoverAction_ = -1;
        redraw();
        return 0;
    case WM_SETCURSOR:
        if (reinterpret_cast<HWND>(wParam) == window_ && LOWORD(lParam) == HTCLIENT &&
            page_ == LauncherPage::Mods && modsHoverAction_ >= 0) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        return DefWindowProc(window_, message, wParam, lParam);
    case WM_LBUTTONUP:
        if (page_ == LauncherPage::Mods) handleModsClick(toContent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        return 0;
    case WM_MOUSEWHEEL:
    {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(window_, &point);
        scrollContent(GET_WHEEL_DELTA_WPARAM(wParam), toContent(point.x, point.y));
        return 0;
    }
    case WM_DPICHANGED:
    {
        const auto rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window_, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO:
    {
        auto limits = reinterpret_cast<MINMAXINFO*>(lParam);
        float scale = static_cast<float>(GetDpiForWindow(window_)) / 96.0F;
        MONITORINFO info{sizeof(info)};
        if (GetMonitorInfo(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &info)) {
            scale = std::min({scale, static_cast<float>(info.rcWork.right - info.rcWork.left) / WindowWidth,
                static_cast<float>(info.rcWork.bottom - info.rcWork.top) / WindowHeight});
        }
        limits->ptMinTrackSize = {static_cast<LONG>(MinimumWindowWidth * scale), static_cast<LONG>(MinimumWindowHeight * scale)};
        return 0;
    }
    case WM_SETTINGCHANGE:
        updateMotionPreference();
        redraw();
        return 0;
    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        if (page_ == LauncherPage::Settings && SettingsPage::containsId(id)) {
            if (notification == BN_CLICKED && id == SettingsPage::Save) saveSettings();
            else settingsPage_.handle(id, notification);
            return 0;
        }
        if (id == NicknameInputId) {
            if (notification == EN_CHANGE) scheduleSkinLookup();
            if (notification == EN_CHANGE || notification == EN_SETFOCUS || notification == EN_KILLFOCUS) redraw();
            return 0;
        }
        if (id == SkinSearchInputId) {
            if (notification == EN_SETFOCUS || notification == EN_KILLFOCUS) {
                markContent(Layout::SkinsSearch, 4);
                redraw(true);
            }
            return 0;
        }
        if (id == LoginUserInputId || id == LoginPasswordInputId || id == LoginConfirmInputId) {
            if (notification == EN_CHANGE && !loginError_.empty()) { loginError_.clear(); redraw(); }
            if (notification == EN_SETFOCUS || notification == EN_KILLFOCUS) {
                markContent(controlBounds(id), 4);
                redraw(true);
            }
            return 0;
        }
        if (notification == BN_CLICKED) handleCommand(id, notification);
        return 0;
    }
    case WM_HSCROLL:
        if (page_ == LauncherPage::Settings) settingsPage_.handleMemoryScroll(reinterpret_cast<HWND>(lParam));
        return 0;
    case WM_DRAWITEM:
    {
        const auto item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item->CtlType != ODT_BUTTON) return DefWindowProc(window_, message, wParam, lParam);
        if (!prepareFrame()) return TRUE;
        RECT button{};
        GetWindowRect(item->hwndItem, &button);
        MapWindowPoints(nullptr, window_, reinterpret_cast<POINT*>(&button), 2);
        // A plain pixel copy: the button shows exactly what the frame drew under it.
        BitBlt(item->hDC, 0, 0, button.right - button.left, button.bottom - button.top, frameDc_, button.left, button.top, SRCCOPY);
        return TRUE;
    }
    case WM_TIMER:
        if (wParam == GameProcessTimerId) pollGameProcess();
        else if (wParam == AnimationTimerId) animate();
        else if (wParam == InstallationProgressTimerId) consumeInstallationProgress();
        else if (wParam == PlaytimeTimerId) { flushPlaytime(); updateTrayTooltip(); redraw(); }
        else if (wParam == LeaderboardTimerId) fetchLeaderboardAsync();
        else if (wParam == SkinLookupTimerId) { KillTimer(window_, SkinLookupTimerId); startSkinLookup(); redraw(); }
        else if (wParam == ServerRetryTimerId) {
            // Quietly check whether the file server came back; the state only changes on an answer.
            if (serverOffline_ && !fileServerPending_ && viewState_.playEnabled && !updatingLauncher_ &&
                !installation_.running && !gameRunning()) checkFileServerAsync();
        }
        return 0;
    case TrayIconMessage:
        switch (LOWORD(lParam)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_LBUTTONDBLCLK:
            restoreFromTray();
            break;
        case WM_CONTEXTMENU:
            showTrayMenu({GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)});
            break;
        default:
            break;
        }
        return 0;
    case InstallationProgressMessage:
        consumeInstallationProgress();
        return 0;
    case NewsResultMessage:
    {
        std::unique_ptr<NewsFeedResult> result(reinterpret_cast<NewsFeedResult*>(lParam));
        if (newsThread_.joinable()) newsThread_.join();
        newsLoading_ = false;
        if (result) {
            std::vector<NewsItem> items;
            if (result->success) { items = std::move(result->items); newsError_.clear(); }
            else newsError_ = L"Não foi possível carregar os avisos do servidor agora.";
            items.push_back(NewsFeed::releaseNotes());
            viewState_.news = std::move(items);
            newsFirst_ = 0;
            applyNewsReadState();
            // Arriving while the list is open counts as read.
            if (page_ == LauncherPage::News) markNewsRead();
        }
        redraw();
        return 0;
    }
    case SkinEventMessage:
    {
        std::unique_ptr<SkinEvent> event(reinterpret_cast<SkinEvent*>(lParam));
        if (!event) return 0;
        if (event->kind >= SkinEvent::Kind::ExploreGroup) {
            handleExploreEvent(*event);
        } else if (event->kind == SkinEvent::Kind::Current) {
            if (skinFetchThread_.joinable()) skinFetchThread_.join();
            currentSkinLoading_ = false;
            if (event->skin) {
                currentSkinKnown_ = true;
                currentSkinSlim_ = event->skin->slim;
                currentSkinPng_ = event->skin->png;
                currentSkin_ = event->skin->custom ? SkinRenderer::decode(event->skin->png) : std::nullopt;
                if (skinSelected_ == 0) skinSlimChoice_ = currentSkinSlim_;
            }
        } else {
            if (skinActionThread_.joinable()) skinActionThread_.join();
            skinBusy_ = false;
            if (event->success) {
                setSkinsMessage(event->kind == SkinEvent::Kind::Upload ? L"Skin trocada! Ela aparece no servidor na próxima vez que você entrar."
                                                                       : L"Pronto: você voltou para a skin padrão.");
                // The avatar and the "in use" card follow the new skin.
                skins_.erase(skinKey(avatarCacheName()));
                startEternalSkinLookup();
                fetchCurrentSkin();
                skinSelected_ = 0;
            } else {
                setSkinsMessage(event->error, true);
            }
        }
        redraw();
        return 0;
    }
    case EternalFirstAccessMessage:
    {
        std::unique_ptr<EternalAccount::FirstAccess> result(reinterpret_cast<EternalAccount::FirstAccess*>(lParam));
        if (eternalThread_.joinable()) eternalThread_.join();
        loginBusy_ = false;
        if (!result || !loginOpen_) { redraw(); return 0; }
        if (result->pending) {
            loginFirstName_ = result->name;
            setLoginMode(1);
        } else {
            loginError_ = !result->errorMessage.empty() ? result->errorMessage : L"Digite sua senha. Ainda não tem conta? Use Criar conta.";
            if (loginPassword_) SetFocus(loginPassword_);
            redraw();
        }
        return 0;
    }
    case EternalAuthMessage:
    {
        std::unique_ptr<EternalAuthResult> result(reinterpret_cast<EternalAuthResult*>(lParam));
        if (eternalThread_.joinable()) eternalThread_.join();
        loginBusy_ = false;
        if (!result) return 0;
        if (!result->success) {
            loginError_ = result->errorMessage;
            if (loginMode_ == 0 && loginPassword_) { SetWindowTextW(loginPassword_, L""); SetFocus(loginPassword_); }
            redraw();
            return 0;
        }
        eternalSession_ = std::move(result->session);
        if (!EternalSessionStore::save(LauncherSettingsStore::dataDirectory(), eternalSession_))
            MessageBoxW(window_, L"Você entrou, mas não foi possível lembrar a sessão. Na próxima abertura será preciso entrar de novo.",
                L"Conta Eternal", MB_OK | MB_ICONWARNING);
        const bool play = playAfterLogin_;
        closeLogin();
        startSkinLookup();
        fetchLeaderboardAsync();
        if (play) prepareEternalPlay();
        redraw();
        return 0;
    }
    case EternalReadyMessage:
    {
        std::unique_ptr<EternalReady> ready(reinterpret_cast<EternalReady*>(lParam));
        if (eternalThread_.joinable()) eternalThread_.join();
        eternalPreparing_ = false;
        if (!ready) return 0;
        if (!ready->auth.success) {
            viewState_.playEnabled = true;
            viewState_.primaryActionLabel = L"JOGAR";
            if (ready->auth.rejected) {
                // The token is gone for good: sign out locally and ask for the password.
                EternalSessionStore::clear(LauncherSettingsStore::dataDirectory());
                const auto name = eternalSession_.name;
                eternalSession_ = {};
                viewState_.operation = LauncherOperation::ReadyToPlay;
                viewState_.headline = L"Entre de novo";
                viewState_.operationMessage = ready->auth.errorMessage;
                openLogin(true, L"Sua sessão expirou. Entre de novo para jogar.");
                if (loginUser_) SetWindowTextW(loginUser_, name.c_str());
                if (loginPassword_) SetFocus(loginPassword_);
            } else {
                viewState_.operation = LauncherOperation::AuthenticationError;
                viewState_.headline = L"Conta Eternal indisponível";
                viewState_.operationMessage = ready->auth.errorMessage;
                viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
            }
            redraw();
            return 0;
        }
        if (ready->agent.empty()) {
            viewState_.operation = LauncherOperation::DownloadError;
            viewState_.headline = L"Falta um componente";
            viewState_.operationMessage = ready->error;
            viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
            viewState_.playEnabled = true;
            redraw();
            return 0;
        }
        // A refreshed token replaces the stored one.
        if (ready->auth.session.accessToken != eternalSession_.accessToken) {
            eternalSession_ = ready->auth.session;
            (void)EternalSessionStore::save(LauncherSettingsStore::dataDirectory(), eternalSession_);
        }
        viewState_.playEnabled = true;
        pendingNickname_ = eternalSession_.name;
        pendingIdentity_ = MinecraftIdentity{eternalSession_.name, eternalSession_.uuid,
            std::wstring(eternalSession_.accessToken.begin(), eternalSession_.accessToken.end()), {}, {},
            EternalAccount::agentArgument(ready->agent, accountsBaseUrl_)};
        startGame();
        return 0;
    }
    case GalleryEventMessage:
    {
        std::unique_ptr<GalleryEvent> event(reinterpret_cast<GalleryEvent*>(lParam));
        if (event) handleGalleryEvent(*event);
        return 0;
    }
    case ScreenshotImageMessage:
    {
        std::unique_ptr<ScreenshotImage> image(reinterpret_cast<ScreenshotImage*>(lParam));
        if (!image) {
            // The worker went through its list; anything that became visible since gets a turn.
            if (screenshotThread_.joinable()) screenshotThread_.join();
            screenshotLoading_ = false;
            requestScreenshotImages();
            redraw();
            return 0;
        }
        std::shared_ptr<Gdiplus::Bitmap> bitmap(image->bitmap.release());
        if (image->large) {
            if (const auto* current = currentScreenshot(); current && current->path.wstring() == image->path) {
                viewerImage_ = bitmap;
                viewerImagePath_ = image->path;
                viewerWidth_ = image->width;
                viewerHeight_ = image->height;
                if (!viewerImage_) setScreenshotsMessage(L"Não foi possível abrir esta captura.", true);
            }
        } else {
            // A failed decode still stores an entry so it is not retried in a loop.
            screenshotThumbnails_[image->path] = bitmap;
        }
        if (page_ == LauncherPage::Screenshots) redraw();
        return 0;
    }
    case PlaytimeSyncMessage:
    {
        std::unique_ptr<PlaytimeSyncUpdate> update(reinterpret_cast<PlaytimeSyncUpdate*>(lParam));
        if (!update) return 0;
        if (update->totalSeconds) { serverTotalSeconds_ = update->totalSeconds; serverTotalName_ = update->name; checkMilestone(); }
        if (update->finished && !playSession_.active()) {
            if (playtimeSyncThread_.joinable()) playtimeSyncThread_.join();
            fetchLeaderboardAsync();
        }
        redraw();
        return 0;
    }
    case SkinLookupResultMessage:
    {
        std::unique_ptr<SkinLookupResult> result(reinterpret_cast<SkinLookupResult*>(lParam));
        if (!result) return 0;
        const auto key = skinKey(result->name);
        if (skinPending_ == key) skinPending_.clear();
        skins_[key] = std::move(*result);
        redraw();
        return 0;
    }
    case LeaderboardResultMessage:
    {
        std::unique_ptr<LeaderboardResult> result(reinterpret_cast<LeaderboardResult*>(lParam));
        if (leaderboardThread_.joinable()) leaderboardThread_.join();
        leaderboardPending_ = false;
        if (result && result->success) {
            leaderboard_ = std::move(*result);
            rankingState_ = RankingState::Ready;
            rankingScroll_ = std::min(rankingScroll_, HomeView::maximumRankingScroll(leaderboard_.players.size()));
            if (leaderboard_.hasYou) {
                serverTotalSeconds_ = leaderboard_.yourSeconds;
                serverTotalName_ = leaderboard_.playerName;
                checkMilestone();
            }
        } else if (rankingState_ == RankingState::Loading) {
            rankingState_ = RankingState::Unavailable;
        }
        redraw();
        return 0;
    }
    case LauncherUpdateProgressMessage:
    {
        std::unique_ptr<LauncherUpdateProgress> progress(reinterpret_cast<LauncherUpdateProgress*>(lParam));
        if (progress) applyLauncherUpdateProgress(*progress);
        return 0;
    }
    case LauncherUpdateResultMessage:
    {
        std::unique_ptr<LauncherUpdateResult> result(reinterpret_cast<LauncherUpdateResult*>(lParam));
        if (!result) return 0;
        if (result->restart) { PostMessageW(window_, WM_CLOSE, 0, 0); return 0; }
        if (result->updateAvailable) {
            updatingLauncher_ = false;
            updateWarning_.clear();
            viewState_.progressVisible = false;
            viewState_.progressPercent = 0;
            viewState_.operation = LauncherOperation::UpdateAvailable;
            viewState_.headline = L"Atualização disponível";
            viewState_.operationMessage = L"Uma nova versão do launcher está pronta para baixar.";
            viewState_.primaryActionLabel = L"AGUARDANDO";
            viewState_.playEnabled = false;
            redraw();

            std::wstring prompt = L"Há uma nova versão do Eternal Launcher disponível";
            if (!result->version.empty()) {
                prompt += L" (v" + result->version;
                if (!result->codename.empty()) prompt += L"-" + result->codename;
                prompt += L")";
            }
            prompt += L".\n\nClique em OK para baixar e instalar agora.\nClique em Cancelar para continuar usando a versão atual.";
            const int answer = MessageBoxW(window_, prompt.c_str(), L"Atualização disponível",
                MB_OKCANCEL | MB_ICONINFORMATION | MB_DEFBUTTON1);
            if (answer == IDOK) {
                checkLauncherUpdateAsync(true);
            } else {
                viewState_.playEnabled = true;
                viewState_.operation = LauncherOperation::CheckingUpdates;
                viewState_.headline = L"Conferindo a instância";
                viewState_.operationMessage = L"Verificando a instalação e os mods...";
                viewState_.primaryActionLabel = L"JOGAR";
                checkFileServerAsync();
                redraw();
            }
            return 0;
        }
        updatingLauncher_ = false;
        updateWarning_ = result->warning;
        viewState_.progressVisible = false;
        viewState_.progressPercent = 0;
        viewState_.operation = LauncherOperation::CheckingUpdates;
        viewState_.headline = L"Conferindo a instância";
        viewState_.operationMessage = L"Verificando a instalação e os mods...";
        checkFileServerAsync();
        redraw();
        return 0;
    }
    case FileServerResultMessage:
    {
        // Ownership of the result moves to the UI thread at this point.
        std::unique_ptr<FileServerManifestResponse> response(reinterpret_cast<FileServerManifestResponse*>(lParam));
        fileServerPending_ = false;
        if (response) applyFileServerResult(*response);
        return 0;
    }
    case MicrosoftDeviceCodeMessage:
    {
        std::unique_ptr<MicrosoftDeviceCode> code(reinterpret_cast<MicrosoftDeviceCode*>(lParam));
        if (code) {
            const std::wstring prompt = L"Abra " + code->url + L" no navegador e digite o código:\n\n" + code->code;
            MessageBox(window_, prompt.c_str(), L"Entrar com Microsoft", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }
    case MicrosoftAuthResultMessage:
    {
        std::unique_ptr<MicrosoftAuthResult> result(reinterpret_cast<MicrosoftAuthResult*>(lParam));
        if (microsoftAuthThread_.joinable()) microsoftAuthThread_.join();
        if (result && result->success) {
            microsoftIdentity_ = result->identity;
            pendingIdentity_ = microsoftIdentity_;
            startSkinLookup();
            pendingNickname_ = microsoftIdentity_.name;
            redraw();
            startGame();
        } else {
            viewState_.operation = LauncherOperation::AuthenticationError;
            viewState_.headline = L"Falha no login Microsoft";
            viewState_.operationMessage = result ? result->message : L"Falha desconhecida.";
            viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
            viewState_.playEnabled = true;
            redraw();
            MessageBox(window_, viewState_.operationMessage.c_str(), L"Login Microsoft", MB_OK | MB_ICONWARNING);
        }
        return 0;
    }
    case WM_CTLCOLORBTN:
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(13, 14, 20));
        return reinterpret_cast<LRESULT>(backgroundBrush_);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    {
        // WM_CTLCOLORSTATIC reaches the parent for disabled and read-only EDIT controls.
        const HDC context = reinterpret_cast<HDC>(wParam);
        const bool nickname = reinterpret_cast<HWND>(lParam) == nicknameInput_;
        const bool disabled = message == WM_CTLCOLORSTATIC;
        SetTextColor(context, disabled ? RGB(150, 151, 172) : RGB(245, 247, 250));
        if (nickname) { SetBkColor(context, RGB(22, 23, 33)); return reinterpret_cast<LRESULT>(surfaceBrush_); }
        if (disabled) { SetBkColor(context, RGB(27, 28, 40)); return reinterpret_cast<LRESULT>(sunkenBrush_); }
        SetBkColor(context, RGB(30, 31, 44));
        return reinterpret_cast<LRESULT>(inputBrush_);
    }
    case WM_QUERYENDSESSION:
        flushPlaytime();
        return TRUE;
    case WM_ENDSESSION:
        if (wParam) flushPlaytime();
        return 0;
    case WM_CLOSE:
        if (installation_.running) {
            closing_ = true;
            cancelInstallation();
            return 0; // Continue pumping messages until the installation's terminal event.
        }
        if (!forceExit_ && trayIcon_.visible() && gameRunning()) {
            // Closing while Minecraft runs keeps counting play time from the notification area.
            hideToTray(true);
            return 0;
        }
        shutdown();
        return 0;
    case WM_DESTROY:
        launcherUpdateThread_.request_stop();
        KillTimer(window_, GameProcessTimerId);
        KillTimer(window_, AnimationTimerId);
        KillTimer(window_, InstallationProgressTimerId);
        KillTimer(window_, PlaytimeTimerId);
        KillTimer(window_, ServerRetryTimerId);
        KillTimer(window_, LeaderboardTimerId);
        trayIcon_.hide();
        fileServerThread_.request_stop();
        installationThread_.request_stop();
        microsoftAuthThread_.request_stop();
        newsThread_.request_stop();
        leaderboardThread_.request_stop();
        playtimeSyncThread_.request_stop();
        PostQuitMessage(0);
        return 0;
    default:
        if (message == TrayIcon::taskbarCreatedMessage()) { trayIcon_.restore(); return 0; }
        return DefWindowProc(window_, message, wParam, lParam);
    }
}

void MainPage::shutdown()
{
    installationThread_.request_stop();
    fileServerThread_.request_stop();
    microsoftAuthThread_.request_stop();
    newsThread_.request_stop();
    if (installationThread_.joinable()) installationThread_.join();
    if (fileServerThread_.joinable()) fileServerThread_.join();
    if (microsoftAuthThread_.joinable()) microsoftAuthThread_.join();
    if (newsThread_.joinable()) newsThread_.join();
    leaderboardThread_.request_stop();
    if (leaderboardThread_.joinable()) leaderboardThread_.join();
    launcherUpdateThread_.request_stop();
    if (launcherUpdateThread_.joinable()) launcherUpdateThread_.join();
    // Drain ownership-bearing messages before the HWND disappears.
    MSG pending{};
    while (PeekMessageW(&pending, window_, LauncherUpdateProgressMessage, LauncherUpdateResultMessage, PM_REMOVE)) {
        if (pending.message == LauncherUpdateProgressMessage) delete reinterpret_cast<LauncherUpdateProgress*>(pending.lParam);
        else delete reinterpret_cast<LauncherUpdateResult*>(pending.lParam);
    }
    while (PeekMessageW(&pending, window_, FileServerResultMessage, MicrosoftAuthResultMessage, PM_REMOVE)) {
        if (pending.message == FileServerResultMessage) delete reinterpret_cast<FileServerManifestResponse*>(pending.lParam);
        else if (pending.message == MicrosoftDeviceCodeMessage) delete reinterpret_cast<MicrosoftDeviceCode*>(pending.lParam);
        else if (pending.message == MicrosoftAuthResultMessage) delete reinterpret_cast<MicrosoftAuthResult*>(pending.lParam);
    }
    while (PeekMessageW(&pending, window_, NewsResultMessage, NewsResultMessage, PM_REMOVE))
        delete reinterpret_cast<NewsFeedResult*>(pending.lParam);
    // Also save when the user closes without clicking Play.
    const auto nickname = nicknameText();
    if (NicknameStore::isValid(nickname)) (void)NicknameStore::save(LauncherSettingsStore::dataDirectory(), nickname);
    finishPlaySession();
    // The worker closes the server session within a couple of seconds at most.
    if (playtimeSyncThread_.joinable()) playtimeSyncThread_.join();
    while (PeekMessageW(&pending, window_, PlaytimeSyncMessage, LeaderboardResultMessage, PM_REMOVE)) {
        if (pending.message == PlaytimeSyncMessage) delete reinterpret_cast<PlaytimeSyncUpdate*>(pending.lParam);
        else delete reinterpret_cast<LeaderboardResult*>(pending.lParam);
    }
    DestroyWindow(window_);
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------

void MainPage::paint()
{
    PAINTSTRUCT ps{};
    const HDC dc = BeginPaint(window_, &ps);
    if (prepareFrame()) {
        // Only the invalidated part is copied; the frame already holds finished pixels.
        const auto& area = ps.rcPaint;
        BitBlt(dc, area.left, area.top, area.right - area.left, area.bottom - area.top, frameDc_, area.left, area.top, SRCCOPY);
    }
    EndPaint(window_, &ps);
}

void MainPage::releaseFrame()
{
    frame_.reset();
    if (frameDc_) {
        if (frameOldBitmap_) SelectObject(frameDc_, frameOldBitmap_);
        DeleteDC(frameDc_);
    }
    if (frameBitmap_) DeleteObject(frameBitmap_);
    frameDc_ = nullptr;
    frameBitmap_ = nullptr;
    frameOldBitmap_ = nullptr;
}

void MainPage::markRect(RECT rect)
{
    if (frameDirty_) return;
    rect.left = std::max(0L, rect.left);
    rect.top = std::max(0L, rect.top);
    rect.right = std::min(static_cast<LONG>(clientWidth_), rect.right);
    rect.bottom = std::min(static_cast<LONG>(clientHeight_), rect.bottom);
    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    // Many small pieces cost more than one full pass.
    if (dirty_.size() >= 24 || invalid_.size() >= 24) {
        frameDirty_ = invalidateAll_ = true;
        dirty_.clear();
        invalid_.clear();
        return;
    }
    dirty_.push_back(rect);
    invalid_.push_back(rect);
}

void MainPage::markContent(const Gdiplus::RectF& b, float inflate)
{
    const float s = viewScale_;
    markRect({static_cast<LONG>(std::floor(offsetX_ + (b.X - inflate) * s)) - 1, static_cast<LONG>(std::floor(offsetY_ + (b.Y - inflate) * s)) - 1,
              static_cast<LONG>(std::ceil(offsetX_ + (b.GetRight() + inflate) * s)) + 1,
              static_cast<LONG>(std::ceil(offsetY_ + (b.GetBottom() + inflate) * s)) + 1});
}

void MainPage::markChrome(const Gdiplus::RectF& b, float inflate)
{
    const float s = viewScale_;
    markRect({static_cast<LONG>(std::floor((b.X - inflate) * s)) - 1, static_cast<LONG>(std::floor((b.Y - inflate) * s)) - 1,
              static_cast<LONG>(std::ceil((b.GetRight() + inflate) * s)) + 1, static_cast<LONG>(std::ceil((b.GetBottom() + inflate) * s)) + 1});
}

void MainPage::markControl(int id)
{
    // Room for focus rings, lift shadows and the Play halo around the control itself.
    if (chromeControl(id)) markChrome(controlBounds(id), 3);
    else markContent(controlBounds(id), 20);
}

float MainPage::playProgressTarget() const
{
    const bool busy = !viewState_.playEnabled && viewState_.operation != LauncherOperation::MinecraftRunning &&
        viewState_.operation != LauncherOperation::UpdateAvailable;
    if (installation_.visible && installation_.running) return InstallationPanelComponent::overallPercent(installation_, installationFill_);
    if (busy) return viewState_.progressVisible ? displayedProgress_ : 0;
    return -1;
}

bool MainPage::prepareFrame()
{
    RECT client{};
    GetClientRect(window_, &client);
    if (client.right <= 0 || client.bottom <= 0) return false;
    if (!frame_ || frame_->GetWidth() != static_cast<UINT>(client.right) ||
        frame_->GetHeight() != static_cast<UINT>(client.bottom)) {
        releaseFrame();
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = client.right;
        info.bmiHeader.biHeight = -client.bottom; // top-down, like GDI+ scan lines
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        frameBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        frameDc_ = frameBitmap_ && bits ? CreateCompatibleDC(nullptr) : nullptr;
        if (!frameDc_) { releaseFrame(); return false; }
        frameOldBitmap_ = SelectObject(frameDc_, frameBitmap_);
        frame_ = std::make_unique<Gdiplus::Bitmap>(client.right, client.bottom, client.right * 4, PixelFormat32bppPARGB,
            static_cast<BYTE*>(bits));
        frameDirty_ = true;
    }
    if (frameDirty_ || !dirty_.empty()) {
        // Finish the frame before any child copies its pixels. A child can paint before WM_PAINT.
        Gdiplus::Graphics graphics(frame_.get());
        if (!frameDirty_) {
            Gdiplus::Region region;
            region.MakeEmpty();
            for (const auto& r : dirty_) region.Union(Gdiplus::Rect(r.left, r.top, r.right - r.left, r.bottom - r.top));
            graphics.SetClip(&region);
        }
        render(graphics, static_cast<float>(client.right), static_cast<float>(client.bottom));
        graphics.Flush(Gdiplus::FlushIntentionSync);
        frameDirty_ = false;
        dirty_.clear();
    }
    return true;
}

ControlVisual MainPage::visual(int id, bool preview) const
{
    ControlVisual result;
    result.enabled = controlEnabled(id);
    if (preview) return result;
    const bool keyboardCues = window_ && (SendMessageW(window_, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) == 0;
    if (id == NicknameInputId) {
        // The EDIT always shows its caret; the underline marks focus even after a mouse click.
        result.focused = nicknameInput_ && GetFocus() == nicknameInput_;
        return result;
    }
    const auto* control = findControl(id);
    if (!control || !control->window) return result;
    result.hover = result.enabled ? control->hover : 0;
    result.focused = keyboardCues && GetFocus() == control->window;
    result.pressed = (SendMessageW(control->window, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
    return result;
}

HomeModel MainPage::homeModel(bool preview) const
{
    HomeModel model;
    auto& player = model.player;
    player.state = &viewState_;
    player.originalMode = originalMode_;
    player.microsoftName = microsoftIdentity_.name;
    const auto nickname = preview ? previewEternalName_ : eternalSession_.name;
    player.eternalName = nickname;
    player.nicknameEmpty = nickname.empty();
    player.nicknameValid = NicknameStore::isValid(nickname);
    player.identitySelection = preview ? (originalMode_ ? 1.0F : 0.0F) : identitySelection_;
    if (!preview) {
        const auto name = avatarCacheName();
        const auto* skin = cachedSkin(name);
        player.skinFound = skin && skin->outcome == SkinLookupOutcome::Found;
        if (player.skinFound) player.face = &skin->face;
        player.skinLoading = !skinPending_.empty() && skinKey(name) == skinPending_;
    }
    model.login = loginOpen_;
    model.loginReveal = preview ? (loginOpen_ ? 1.0F : 0.0F) : loginReveal_;
    model.loginBusy = loginBusy_;
    model.loginError = loginError_;
    model.loginNotice = loginNotice_;
    model.loginMode = loginMode_;
    model.loginName = loginFirstName_;
    if (preview && loginOpen_) model.previewLoginUser = previewLoginUser_;
    const auto now = wallClock();
    const auto record = playSession_.active() ? playSession_.snapshot(PlaySession::Clock::now(), now) : playtime_;
    player.playing = playSession_.active() || viewState_.operation == LauncherOperation::MinecraftRunning;
    player.lastPlayed = player.playing ? std::wstring(L"Agora") : PlaytimeStore::formatLastPlayed(record.lastPlayed, now);
    player.totalPlayed = record.totalSeconds > 0 || player.playing ? PlaytimeStore::formatDuration(record.totalSeconds) : std::wstring(L"—");
    // The server's number wins; the local counter is only a fallback while it is unknown.
    const auto account = originalMode_ ? microsoftIdentity_.name : nickname;
    if (!preview && serverTotalSeconds_ && serverTotalName_ == account) {
        player.totalPlayed = PlaytimeStore::formatDuration(*serverTotalSeconds_);
        // Levels follow the server's number only, the same one the ranking uses.
        player.tier = RankTier::level(*serverTotalSeconds_);
        player.tierProgress = RankTier::progress(*serverTotalSeconds_);
    }
    if (preview && previewTier_ > 0) {
        player.tier = previewTier_;
        player.tierProgress = .62F;
    }
    const bool busy = !viewState_.playEnabled && viewState_.operation != LauncherOperation::MinecraftRunning &&
        viewState_.operation != LauncherOperation::UpdateAvailable;
    if (preview) {
        if (installation_.visible && installation_.running) player.progress = InstallationPanelComponent::overallPercent(installation_, -1);
        else if (busy) player.progress = viewState_.progressVisible ? static_cast<float>(viewState_.progressPercent) : 0;
    } else {
        // The button shows an eased value so stage changes glide instead of jumping.
        const float target = playProgressTarget();
        player.progress = target < 0 ? -1 : (playProgressShown_ >= 0 ? playProgressShown_ : target);
    }
    player.phase = animationPhase_;
    player.time = preview ? 1.2F : time_;
    player.motion = preview || motionEnabled_;
    player.ambient = preview || (motionEnabled_ && ambientMotion_ && window_ && GetForegroundWindow() == window_);
    player.playSweep = preview ? 1.0F : playSweep_;
    player.playPulse = preview ? 0.0F : playPulse_;
    player.playLight = playLight_;
    player.playPress = preview ? 0.0F : playPress_;
    player.updateWarning = updateWarning_;
    player.installationVisible = installation_.visible;
    player.serverOffline = serverOffline_ && !vanillaSelected_;
    const auto operation = viewState_.operation;
    if (vanillaSelected_ && viewState_.playEnabled && !installation_.running && !player.playing &&
        (operation == LauncherOperation::ReadyToPlay || operation == LauncherOperation::NeedsInstallation ||
         operation == LauncherOperation::OfflineReady || operation == LauncherOperation::ServerUnavailable)) {
        player.statusHeadline = L"Minecraft 1.8.9";
        player.statusMessage = originalMode_ ? L"Versão pura, em pasta própria. Este tempo não conta no ranking."
                                             : L"Sem conta original, só entra em servidores que aceitam nickname.";
        player.playLabel = L"JOGAR 1.8.9";
    }

    const int serverMods = mods_.manifestKnown ? static_cast<int>(mods_.server.size()) : -1;
    player.versionSummary = vanillaSelected_ ? std::wstring(L"1.8.9 · Sem mods")
        : std::wstring(LauncherConfig::MinecraftVersion) + L" · Forge " + std::wstring(LauncherConfig::ForgeVersion);
    {
        const int memory = settings_.maximumMemoryMegabytes;
        std::wstring gigabytes = memory % 1024 == 0 ? std::to_wstring(memory / 1024) : std::to_wstring(memory / 1024) + L"," + std::to_wstring(memory % 1024 * 10 / 1024);
        player.memorySummary = gigabytes + L" GB" + (vanillaSelected_ ? L" · Java 8" : settings_.javaExecutable.empty() ? L" · Java automático" : L" · Java próprio");
    }
    if (serverMods >= 0) {
        player.modsSummary = std::to_wstring(serverMods) + L" do servidor";
        if (modsScanned_ && !mods_.user.empty()) player.modsSummary += L" · " + std::to_wstring(mods_.user.size()) + (mods_.user.size() == 1 ? L" seu" : L" seus");
    }
    model.chips = {L"Minecraft " + std::wstring(LauncherConfig::MinecraftVersion), L"Forge " + std::wstring(LauncherConfig::ForgeVersion),
        serverMods >= 0 ? std::to_wstring(serverMods) + L" mods" : std::wstring(L"Modpack do grupo")};
    if (vanillaSelected_) {
        player.modsSummary = L"Sem mods";
        model.chips = {L"Minecraft 1.8.9", L"Sem mods", L"Java 8"};
    }
    model.reveal = preview ? 1 : portalEntrance_;
    model.energy = preview || !motionEnabled_ ? 0 : std::max({portalEnergy_, portalBurst_, .28F});
    // Under the installation screen the landscape holds still: only the screen is repainted.
    model.phase = installation_.visible && !preview ? .45F : animationPhase_;
    model.drift = preview || installation_.visible ? 0 : particlePhase_;
    model.modpackChanges = modpackChangesVisible_ ? &modpackChanges_ : nullptr;
    model.levels = levelsOpen_;
    model.levelsReveal = preview ? (levelsOpen_ ? 1.0F : 0.0F) : levelsReveal_;
    if (preview) {
        model.levelsKnown = previewLevelsSeconds_ > 0;
        model.levelsSeconds = previewLevelsSeconds_;
    } else if (const auto seconds = serverSecondsForAccount()) {
        model.levelsKnown = true;
        model.levelsSeconds = *seconds;
    }
    model.celebration = celebration_;
    model.celebrationSeconds = celebrationSeconds_;
    model.celebrationReveal = preview ? (celebration_ > 0 ? 1.0F : 0.0F) : celebrationReveal_;
    {
        const int spotlight = spotlightIndex();
        model.spotlightReveal = preview ? (spotlight >= 0 ? 1.0F : 0.0F) : spotlightReveal_;
        model.spotlight = spotlight >= 0 && model.spotlightReveal > .01F && !installation_.visible;
        if (spotlight >= 0) model.spotlightItem = viewState_.news[static_cast<std::size_t>(spotlight)];
        model.unreadNews = unreadNewsCount();
    }
    model.entrance = preview ? 1.0F : std::min(pageFade_, portalEntrance_);
    model.serverMods = serverMods;
    model.userMods = modsScanned_ ? static_cast<int>(mods_.user.size()) : -1;
    if (preview) {
        // Preview players span the play-time levels of the podium.
        constexpr std::uint64_t hour = 3600;
        const std::uint64_t seconds[] = {previewRankingHours_[0] * hour, previewRankingHours_[1] * hour, previewRankingHours_[2] * hour + 20 * 60,
            71 * hour + 5 * 60, 44 * hour};
        const wchar_t* names[] = {L"Aurora_", L"Pedrinho", L"Steve", L"LunaCraft", L"Tico_BR"};
        for (int i = 0; i < 5; ++i)
            model.ranking.push_back({i + 1, names[i], PlaytimeStore::formatDuration(seconds[i]), i == 0 || i == 2, i == 2, seconds[i]});
        for (int i = 0; i < previewExtraPlayers_; ++i) {
            const std::uint64_t extra = (40 - static_cast<std::uint64_t>(i) * 3) * hour;
            model.ranking.push_back({i + 6, L"Jogador" + std::to_wstring(i + 1), PlaytimeStore::formatDuration(extra), false, false, extra});
        }
        model.rankingYou = L"Você · #3 · " + model.ranking[2].time + L" · Nível " + RankTier::name(RankTier::level(model.ranking[2].seconds));
        model.rankingScroll = previewRankingScroll_;
    } else if (rankingState_ == RankingState::Ready) {
        for (const auto& entry : leaderboard_.players)
            model.ranking.push_back({entry.rank, entry.name, PlaytimeStore::formatDuration(entry.totalSeconds), entry.online,
                !account.empty() && _wcsicmp(entry.name.c_str(), account.c_str()) == 0, entry.totalSeconds});
        model.rankingNotice = L"Ninguém no ranking ainda. Seja o primeiro!";
        model.rankingScroll = rankingScroll_;
        model.rankingYou = leaderboard_.hasYou && leaderboard_.playerName == account
            ? L"Você · #" + std::to_wstring(leaderboard_.yourRank) + L" · " + PlaytimeStore::formatDuration(leaderboard_.yourSeconds) +
              L" · Nível " + RankTier::name(RankTier::level(leaderboard_.yourSeconds))
            : std::wstring(L"Jogue para entrar no ranking");
    } else {
        model.rankingNotice = rankingState_ == RankingState::Loading ? L"Carregando ranking…" : L"Ranking indisponível no momento.";
    }
    return model;
}

ModsModel MainPage::modsModel() const
{
    ModsModel model;
    model.inventory = &mods_;
    model.serverScroll = serverScroll_;
    model.userScroll = userScroll_;
    model.hoveredRow = modsHoverRow_;
    model.hoveredAction = modsHoverAction_;
    model.message = modsMessage_;
    model.messageError = modsMessageError_;
    model.locked = gameRunning() || installation_.running;
    model.knobs = &modKnobs_;
    return model;
}

void MainPage::render(Gdiplus::Graphics& g, float width, float height, bool preview)
{
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    {
        // FillRectangle honours the clip of a partial repaint.
        Gdiplus::SolidBrush background(Theme::Background);
        g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        g.FillRectangle(&background, 0.0F, 0.0F, width, height);
        g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    }
    const ControlVisuals visuals = [this, preview](int id) { return visual(id, preview); };
    const float s = viewScale_;

    // Content: uniformly scaled and centered, clipped to the area beside the chrome.
    auto saved = g.Save();
    g.SetClip(Gdiplus::RectF(SidebarWidth * s, TitleBarHeight * s, width, height), Gdiplus::CombineModeIntersect);
    // New pages rise into place while they fade in.
    const float settle = preview ? 1.0F : 1 - std::pow(1 - std::clamp(pageFade_, 0.0F, 1.0F), 3.0F);
    g.TranslateTransform(offsetX_, offsetY_ + std::round(16 * (1 - settle) * s));
    g.ScaleTransform(s, s);
    switch (page_) {
    case LauncherPage::Home:
        home_.draw(g, homeModel(preview), hero_, visuals, true);
        if (installation_.visible)
            installationPanel_.draw(g, installation_, animationPhase_, visuals, preview ? -1 : installationFill_,
                preview ? std::array<float, 6>{} : completionPulse_);
        break;
    case LauncherPage::Versions:
        versionsView_.draw(g, VersionsModel{vanillaSelected_, vanillaInstalled_, mods_.manifestKnown ? static_cast<int>(mods_.server.size()) : -1,
            preview ? 1.0F : pageFade_}, hero_, visuals);
        break;
    case LauncherPage::News:
        newsView_.draw(g, NewsModel{&viewState_.news, newsFirst_, newsLoading_, newsError_, preview ? 1.0F : pageFade_}, visuals);
        break;
    case LauncherPage::Mods:
        modsView_.draw(g, modsModel(), visuals);
        break;
    case LauncherPage::Screenshots:
        screenshotsView_.draw(g, screenshotsModel(preview), visuals);
        break;
    case LauncherPage::Skins:
        skinsView_.draw(g, skinsModel(preview), visuals);
        break;
    case LauncherPage::Settings:
        settingsPage_.draw(g, preview);
        break;
    case LauncherPage::Help:
        helpView_.draw(g, HelpModel{helpMessage_, helpMessageError_, canRepair(), preview ? 1.0F : pageFade_}, visuals);
        break;
    }
    if (!preview && pageFade_ < 1) {
        // New pages rise out of the background instead of sliding native controls around.
        const float remaining = 1 - pageFade_;
        g.ResetTransform();
        Gdiplus::SolidBrush veil(Gdiplus::Color(static_cast<BYTE>(std::clamp(remaining * remaining * 255.0F, 0.0F, 255.0F)),
            Theme::Background.GetR(), Theme::Background.GetG(), Theme::Background.GetB()));
        g.FillRectangle(&veil, 0.0F, 0.0F, width, height);
    }
    g.Restore(saved);

    // Chrome: anchored to the window edges, scaled only.
    saved = g.Save();
    g.ScaleTransform(s, s);
    // The badge stays while unread news exist, except on the News page itself.
    const int badge = page_ == LauncherPage::News ? 0 : unreadNewsCount();
    chrome_.draw(g, width / s, height / s, page_, window_ && IsZoomed(window_), windowActive_ || preview, visuals,
        preview ? navigationTarget() : navIndicator_, badge, preview ? 1.0F : motionEnabled_ && page_ == LauncherPage::Home ? time_ : -1.0F);
    g.Restore(saved);
}

void MainPage::updateMotionPreference()
{
    BOOL enabled = TRUE;
    if (SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) motionEnabled_ = enabled != FALSE;
    if (!motionEnabled_) {
        for (auto& control : controls_) control.hover = control.hovered ? 1.0F : 0.0F;
        displayedProgress_ = static_cast<float>(viewState_.progressPercent);
        animationPhase_ = 0.45F;
        portalEntrance_ = 1;
        pageFade_ = 1;
        navIndicator_ = navigationTarget();
        spotlightReveal_ = spotlightTarget();
        celebrationReveal_ = celebrationTarget();
        levelsReveal_ = levelsTarget();
        loginReveal_ = loginTarget();
        playSweep_ = 1; playPulse_ = 0;
        settingsPage_.settle();
    }
}

void MainPage::redraw(bool animationFrame)
{
    if (!window_) return;
    if (pulseAttempt_ != installation_.attempt) {
        pulseAttempt_ = installation_.attempt;
        completionPulse_ = {};
        previousSteps_.fill(InstallationStepState::Waiting);
    }
    for (std::size_t i = 0; i < completionPulse_.size(); ++i) {
        if (installation_.steps[i] != previousSteps_[i] &&
            (installation_.steps[i] == InstallationStepState::Complete || installation_.steps[i] == InstallationStepState::Available))
            completionPulse_[i] = 1;
        previousSteps_[i] = installation_.steps[i];
        if (!installation_.visible) completionPulse_[i] = 0;
    }
    const auto& transfer = installation_.progress;
    const float fillTarget = static_cast<float>(transfer.percent.value_or(0));
    if (fillAttempt_ != installation_.attempt || fillStage_ != transfer.stage || fillTransfer_ != transfer.transferId ||
        transfer.activity != InstallationActivity::Downloading || fillTarget < installationFill_) {
        installationFill_ = 0;
        fillAttempt_ = installation_.attempt; fillStage_ = transfer.stage; fillTransfer_ = transfer.transferId;
    }
    if (!animationFrame) {
        // State changes repaint everything; animation frames only what animate() marked.
        frameDirty_ = invalidateAll_ = true;
        dirty_.clear();
        invalid_.clear();
        syncControls();
        const float progress = playProgressTarget();
        if (progress < 0) playProgressShown_ = -1;
        else if (playProgressShown_ < 0 || progress < playProgressShown_ - .5F) playProgressShown_ = progress;
    }

    bool unsettled = std::abs(displayedProgress_ - static_cast<float>(viewState_.progressPercent)) > 0.1F || pageFade_ < 1;
    for (const auto& control : controls_) unsettled |= std::abs(control.hover - (control.hovered ? 1.0F : 0.0F)) > 0.005F;
    const float selection = originalMode_ ? 1.0F : 0.0F;
    unsettled |= std::abs(identitySelection_ - selection) > .005F;
    const bool transferVisible = installation_.visible && installation_.running && !installation_.cancelling &&
        transfer.activity == InstallationActivity::Downloading && transfer.percent.has_value();
    unsettled |= transferVisible && std::abs(installationFill_ - fillTarget) > .1F;
    for (const float pulse : completionPulse_) unsettled |= pulse > 0;
    const bool pressed = findControl(PlayButtonId) && findControl(PlayButtonId)->window &&
        (SendMessageW(findControl(PlayButtonId)->window, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
    unsettled |= std::abs(navIndicator_ - navigationTarget()) > .75F || std::abs(spotlightReveal_ - spotlightTarget()) > .01F ||
        std::abs(celebrationReveal_ - celebrationTarget()) > .01F || std::abs(levelsReveal_ - levelsTarget()) > .01F ||
        std::abs(loginReveal_ - loginTarget()) > .01F;
    unsettled |= playSweep_ < 1 || playPulse_ > 0 || std::abs(playPress_ - (pressed ? 1.0F : 0.0F)) > .01F ||
        std::abs(playLight_ - playLightTarget_) > .005F;
    for (const auto& mod : mods_.user) {
        const auto knob = modKnobs_.find(mod.name);
        unsettled |= knob != modKnobs_.end() && knob->second != (mod.enabled ? 1.0F : 0.0F);
    }
    if (page_ == LauncherPage::Settings) unsettled |= settingsPage_.advance(0);
    const float progressTarget = playProgressTarget();
    unsettled |= progressTarget >= 0 && std::abs(playProgressShown_ - progressTarget) > .05F;
    if (!motionEnabled_) {
        playProgressShown_ = progressTarget;
        for (auto& control : controls_) control.hover = control.hovered ? 1.0F : 0.0F;
        displayedProgress_ = static_cast<float>(viewState_.progressPercent);
        identitySelection_ = selection;
        installationFill_ = fillTarget;
        portalEntrance_ = 1; portalEnergy_ = 0; portalBurst_ = 0; pageFade_ = 1;
        completionPulse_ = {};
        navIndicator_ = navigationTarget(); spotlightReveal_ = spotlightTarget(); celebrationReveal_ = celebrationTarget();
        levelsReveal_ = levelsTarget();
        loginReveal_ = loginTarget();
        playSweep_ = 1; playPulse_ = 0; playPress_ = pressed ? 1.0F : 0.0F; playLight_ = playLightTarget_;
        for (const auto& mod : mods_.user) modKnobs_[mod.name] = mod.enabled ? 1.0F : 0.0F;
        settingsPage_.settle();
        unsettled = false;
    }
    const bool home = page_ == LauncherPage::Home;
    const bool busy = !viewState_.playEnabled && viewState_.operation != LauncherOperation::MinecraftRunning &&
        viewState_.operation != LauncherOperation::UpdateAvailable;
    // The Play spinner turns whenever work is in progress, with or without a percentage.
    const bool indeterminate = home && (installation_.visible ? installation_.running && !installation_.cancelling : busy);
    const bool hero = home && !installation_.visible;
    const bool ambient = ambientMotion_ && hero && GetForegroundWindow() == window_;
    // Placeholders shimmer while screenshot thumbnails decode.
    const bool screenshotsLoading = page_ == LauncherPage::Screenshots && (galleryTab_ ? galleryWorking_ : screenshotLoading_);
    const bool running = motionEnabled_ && IsWindowVisible(window_) && !IsIconic(window_) && !closing_ &&
        (unsettled || indeterminate || screenshotsLoading || (home && !skinPending_.empty()) ||
         (hero &&(ambient || portalEntrance_ < 1 || portalHovered_ || portalEnergy_ > .005F || portalBurst_ > 0)));
    if (running && !animationTimer_) {
        previousFrame_ = std::chrono::steady_clock::now();
        animationTimer_ = SetTimer(window_, AnimationTimerId, USER_TIMER_MINIMUM, nullptr) != 0;
    } else if (!running && animationTimer_) {
        KillTimer(window_, AnimationTimerId);
        animationTimer_ = false;
    }
    // Hover/focus changes the canvas and owner-drawn buttons, not the EDIT contents or caret.
    if (invalidateAll_) {
        invalidateAll_ = false;
        invalid_.clear();
        RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_NOCHILDREN);
        for (const auto& control : controls_)
            if (control.window && (GetWindowLongPtrW(control.window, GWL_STYLE) & WS_VISIBLE) &&
                control.id != NicknameInputId) InvalidateRect(control.window, nullptr, FALSE);
        if (page_ == LauncherPage::Settings) settingsPage_.invalidateButtons();
        return;
    }
    // Animation frame: present only the pieces that moved, and only the buttons over them.
    if (invalid_.empty()) return;
    for (const auto& area : invalid_) RedrawWindow(window_, &area, nullptr, RDW_INVALIDATE | RDW_NOCHILDREN);
    for (const auto& control : controls_) {
        if (!control.window || control.id == NicknameInputId || !(GetWindowLongPtrW(control.window, GWL_STYLE) & WS_VISIBLE)) continue;
        RECT overlap{};
        for (const auto& area : invalid_)
            if (IntersectRect(&overlap, &area, &control.rect)) { InvalidateRect(control.window, nullptr, FALSE); break; }
    }
    invalid_.clear();
    if (page_ == LauncherPage::Settings) settingsPage_.invalidateButtons();
}

void MainPage::animate()
{
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::clamp(std::chrono::duration<float, std::milli>(now - previousFrame_).count(), 0.0F, 100.0F);
    previousFrame_ = now;
    // Remember what is on screen so only what actually moves gets repainted.
    const float fadeBefore = pageFade_, navBefore = navIndicator_, spotlightBefore = spotlightReveal_, selectionBefore = identitySelection_;
    const float screensBefore[] = {levelsReveal_, loginReveal_, celebrationReveal_};
    const float playBefore[] = {playSweep_, playPulse_, playPress_, playLight_, playProgressShown_, displayedProgress_};
    const float fillBefore = installationFill_, entranceBefore = portalEntrance_;
    const auto pulsesBefore = completionPulse_;
    std::vector<float> hoverBefore;
    hoverBefore.reserve(controls_.size());
    for (const auto& control : controls_) hoverBefore.push_back(control.hover);
    const auto knobsBefore = modKnobs_;
    const bool settingsBefore = page_ == LauncherPage::Settings && settingsPage_.advance(0);
    particlePhase_ = std::fmod(particlePhase_ + dt / 8000, 1.0F);
    const float energyTarget = portalHovered_ && page_ == LauncherPage::Home && !installation_.visible ? 1.0F : 0.0F;
    portalEnergy_ += (energyTarget - portalEnergy_) * (1 - std::exp(-dt / 95));
    if (std::abs(energyTarget - portalEnergy_) < .006F) portalEnergy_ = energyTarget;
    portalBurst_ = std::max(0.0F, portalBurst_ - dt / 1400);
    for (float& pulse : completionPulse_) pulse = std::max(0.0F, pulse - dt / 420);
    const float ease = 1 - std::exp(-dt / 45);
    for (auto& control : controls_) {
        const float target = control.hovered ? 1.0F : 0.0F;
        control.hover += (target - control.hover) * ease;
        if (std::abs(target - control.hover) < 0.006F) control.hover = target;
    }
    const float target = static_cast<float>(viewState_.progressPercent);
    // A new phase can reset progress, so never animate a backwards value.
    if (target < displayedProgress_) displayedProgress_ = target;
    else displayedProgress_ += (target - displayedProgress_) * (1 - std::exp(-dt / 85));
    if (std::abs(target - displayedProgress_) < 0.1F) displayedProgress_ = target;
    const float selection = originalMode_ ? 1.0F : 0.0F;
    identitySelection_ += (selection - identitySelection_) * (1 - std::exp(-dt / 55));
    if (std::abs(selection - identitySelection_) < .006F) identitySelection_ = selection;
    const float fillTarget = static_cast<float>(installation_.progress.percent.value_or(0));
    installationFill_ += (fillTarget - installationFill_) * (1 - std::exp(-dt / 70));
    if (std::abs(fillTarget - installationFill_) < .1F) installationFill_ = fillTarget;
    if (page_ == LauncherPage::Home && !installation_.visible) portalEntrance_ = std::min(1.0F, portalEntrance_ + dt / 750);
    pageFade_ = std::min(1.0F, pageFade_ + dt / 300);
    time_ = std::fmod(time_ + dt / 1000, 3600.0F);
    const auto approach = [dt](float& value, float goal, float tau, float snap) {
        value += (goal - value) * (1 - std::exp(-dt / tau));
        if (std::abs(goal - value) < snap) value = goal;
    };
    approach(navIndicator_, navigationTarget(), 50, .75F);
    approach(spotlightReveal_, spotlightTarget(), 90, .01F);
    approach(celebrationReveal_, celebrationTarget(), 140, .01F);
    approach(levelsReveal_, levelsTarget(), 70, .01F);
    approach(loginReveal_, loginTarget(), 60, .01F);
    playSweep_ = std::min(1.0F, playSweep_ + dt / 560);
    playPulse_ = std::max(0.0F, playPulse_ - dt / 420);
    const auto* play = findControl(PlayButtonId);
    const bool pressed = play && play->window && (SendMessageW(play->window, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
    approach(playPress_, pressed ? 1.0F : 0.0F, 30, .01F);
    approach(playLight_, playLightTarget_, 55, .005F);
    for (const auto& mod : mods_.user) {
        auto& knob = modKnobs_[mod.name];
        const float goal = mod.enabled ? 1.0F : 0.0F;
        knob = knob < goal ? std::min(goal, knob + dt / 200) : std::max(goal, knob - dt / 200);
    }
    const bool settingsMoving = page_ == LauncherPage::Settings && settingsPage_.advance(dt);
    animationPhase_ = std::fmod(animationPhase_ + dt / 1600, 1.0F);
    {
        const float goal = playProgressTarget();
        if (goal < 0) playProgressShown_ = -1;
        else if (playProgressShown_ < 0 || goal < playProgressShown_ - .5F) playProgressShown_ = goal;
        else approach(playProgressShown_, goal, 160, .05F);
    }

    // Mark the regions this frame changed.
    // Pages fade in whole, and a screen's dim layer covers all of Home while it opens or closes.
    if (pageFade_ != fadeBefore || levelsReveal_ != screensBefore[0] || loginReveal_ != screensBefore[1]) frameDirty_ = invalidateAll_ = true;
    if (navIndicator_ != navBefore) markChrome({0, TitleBarHeight, SidebarWidth, chromeHeight() - TitleBarHeight});
    for (std::size_t i = 0; i < controls_.size() && i < hoverBefore.size(); ++i)
        if (controls_[i].hover != hoverBefore[i]) markControl(controls_[i].id);
    if (page_ == LauncherPage::Home) {
        const bool heroVisible = !installation_.visible;
        const bool ambientNow = motionEnabled_ && ambientMotion_ && GetForegroundWindow() == window_;
        const bool working = !viewState_.playEnabled && viewState_.operation != LauncherOperation::MinecraftRunning &&
            viewState_.operation != LauncherOperation::UpdateAvailable;
        const bool playing = playSession_.active() || viewState_.operation == LauncherOperation::MinecraftRunning;
        if (heroVisible) {
            markContent(Layout::Hero, 2);  // particles drift every frame
            if (portalEntrance_ != entranceBefore) markContent(Layout::LeftColumn, 24);  // tiles and cards rise
        }
        if (spotlightReveal_ != spotlightBefore || celebrationReveal_ != screensBefore[2]) markContent(Layout::Hero, 2);
        // The badge rings and the podium shimmers while the window is in front.
        if (ambientNow || working || playing) {
            if (unreadNewsCount() > 0) {
                const auto news = Layout::navButton(static_cast<int>(LauncherPage::News));
                markChrome({news.X, news.Y - 10, news.Width + 16, news.Height}, 8);
            }
            if (rankingState_ == RankingState::Ready)
                for (std::size_t i = 0; i < 3 && i < leaderboard_.players.size(); ++i)
                    if (RankTier::level(leaderboard_.players[i].totalSeconds) >= 3) { markContent(Layout::rankingCard(), 4); break; }
        }
        if (identitySelection_ != selectionBefore)
            markContent({Layout::OfflineMode.X, Layout::OfflineMode.Y, Layout::OriginalMode.GetRight() - Layout::OfflineMode.X,
                Layout::OfflineMode.Height}, 4);
        const float playAfter[] = {playSweep_, playPulse_, playPress_, playLight_, playProgressShown_, displayedProgress_};
        bool playMoved = working || ambientNow;
        for (std::size_t i = 0; i < std::size(playAfter); ++i) playMoved |= playAfter[i] != playBefore[i];
        if (playMoved)
            markContent({Layout::Play.X, Layout::Play.Y, Layout::PlayMenu.GetRight() - Layout::Play.X, Layout::Play.Height}, 16);
        if (ambientNow || playing || !skinPending_.empty()) markContent(Layout::Avatar, 18);
        if (working || playing) markContent({Layout::StatusLine.X - 12, Layout::StatusLine.Y - 4, 32, 32});
        // Status dots pulse while their card is in an "in progress" tone.
        if (motionEnabled_ && viewState_.statusCards.size() >= 3)
            for (int i = 0; i < 3; ++i)
                if (viewState_.statusCards[static_cast<std::size_t>(2 - i)].tone == StatusTone::Information) {
                    const auto tile = Layout::tile(i);
                    markContent({tile.X, tile.Y, 48, 48});
                }
        if (installation_.visible &&
            (installation_.running || installationFill_ != fillBefore || completionPulse_ != pulsesBefore))
            markContent(Layout::Installation, 4);
    }
    if (page_ == LauncherPage::Mods && modKnobs_ != knobsBefore) markContent(Layout::ModsUser, 4);
    if (page_ == LauncherPage::Screenshots && (galleryTab_ ? galleryWorking_ : screenshotLoading_) && screenshotViewer_ < 0)
        markContent({100, 168, 1156, 608}, 4);
    if (settingsBefore || settingsMoving) markContent(Layout::Content, 4);
    redraw(true);
}

// ---------------------------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------------------------

void MainPage::createControls()
{
    for (const auto& spec : ControlSpecs) {
        if (spec.id == NicknameInputId) {
            createNicknameInput();
            controls_.push_back({NicknameInputId, nicknameInput_, false});
            continue;
        }
        if (spec.id == LoginUserInputId || spec.id == LoginPasswordInputId || spec.id == LoginConfirmInputId) {
            const bool password = spec.id != LoginUserInputId;
            const auto edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL | (password ? ES_PASSWORD : 0),
                0, 0, 0, 0, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(spec.id)), instance_, nullptr);
            if (!edit) continue;
            SendMessageW(edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(
                spec.id == LoginUserInputId ? L"Seu nick" : spec.id == LoginPasswordInputId ? L"Sua senha" : L"Repita a senha"));
            SendMessageW(edit, EM_SETLIMITTEXT, password ? 128 : 16, 0);
            SetWindowTheme(edit, L"DarkMode_Explorer", nullptr);
            (spec.id == LoginUserInputId ? loginUser_ : spec.id == LoginPasswordInputId ? loginPassword_ : loginConfirm_) = edit;
            controls_.push_back({spec.id, edit, false});
            continue;
        }
        if (spec.id == SkinSearchInputId) {
            skinSearch_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
                0, 0, 0, 0, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(spec.id)), instance_, nullptr);
            if (!skinSearch_) continue;
            SendMessageW(skinSearch_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Buscar skin por nick"));
            SendMessageW(skinSearch_, EM_SETLIMITTEXT, 16, 0);
            SetWindowTheme(skinSearch_, L"DarkMode_Explorer", nullptr);
            controls_.push_back({spec.id, skinSearch_, false});
            continue;
        }
        const DWORD style = WS_CHILD | BS_OWNERDRAW | (spec.tabStop ? WS_TABSTOP : 0);
        const auto window = CreateWindowExW(0, L"BUTTON", spec.label, style, 0, 0, 0, 0, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(spec.id)), instance_, nullptr);
        if (!window) continue;
        SetWindowSubclass(window, controlProcedure, static_cast<UINT_PTR>(spec.id), reinterpret_cast<DWORD_PTR>(this));
        controls_.push_back({spec.id, window, spec.chrome});
    }
}

void MainPage::createNicknameInput()
{
    const auto savedNickname = NicknameStore::load(LauncherSettingsStore::dataDirectory());
    nicknameInput_ = CreateWindowEx(0, L"EDIT", savedNickname.c_str(), WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(NicknameInputId)), instance_, nullptr);
    SendMessage(nicknameInput_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Seu nickname"));
    SendMessage(nicknameInput_, EM_SETLIMITTEXT, 16, 0);
    SendMessage(nicknameInput_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    SetWindowTheme(nicknameInput_, L"DarkMode_Explorer", nullptr);
    scheduleSkinLookup();  // show the saved nickname's skin shortly after opening
}

MainPage::Control* MainPage::findControl(int id)
{
    for (auto& control : controls_) if (control.id == id) return &control;
    return nullptr;
}

const MainPage::Control* MainPage::findControl(int id) const
{
    for (const auto& control : controls_) if (control.id == id) return &control;
    return nullptr;
}

bool MainPage::chromeControl(int id) const
{
    return id == MinimizeButtonId || id == MaximizeButtonId || id == CloseButtonId || inRange(id, NavHomeId, NavigationCount);
}

float MainPage::chromeWidth() const { return viewScale_ > 0 ? clientWidth_ / viewScale_ : static_cast<float>(WindowWidth); }
float MainPage::chromeHeight() const { return viewScale_ > 0 ? clientHeight_ / viewScale_ : static_cast<float>(WindowHeight); }

float MainPage::navigationTarget() const
{
    if (page_ == LauncherPage::Help) return Layout::sidebarHelp(chromeHeight()).Y;
    return Layout::navButton(static_cast<int>(page_)).Y;
}

float MainPage::spotlightTarget() const
{
    return spotlightIndex() >= 0 && celebration_ == 0 && !modpackChangesVisible_ && !levelsOpen_ && !loginOpen_ && page_ == LauncherPage::Home &&
        !installation_.visible ? 1.0F : 0.0F;
}

float MainPage::celebrationTarget() const
{
    return celebration_ > 0 && !modpackChangesVisible_ && !levelsOpen_ && !loginOpen_ && page_ == LauncherPage::Home && !installation_.visible ? 1.0F : 0.0F;
}

float MainPage::levelsTarget() const
{
    return levelsOpen_ && page_ == LauncherPage::Home && !installation_.visible ? 1.0F : 0.0F;
}

void MainPage::showLevels(bool open)
{
    if (levelsOpen_ == open) return;
    levelsOpen_ = open;
    redraw();
    // Closing returns focus to the card that opened it.
    if (!open)
        if (const auto* stat = findControl(LevelsButtonId); stat && stat->window && GetFocus() != stat->window) {
            const HWND focus = GetFocus();
            if (!focus || !(GetWindowLongPtrW(focus, GWL_STYLE) & WS_VISIBLE)) SetFocus(stat->window);
        }
}

void MainPage::syncModKnobs()
{
    // New mods start at their state; known ones keep sliding toward it.
    std::map<std::wstring, float> knobs;
    for (const auto& mod : mods_.user) {
        const auto found = modKnobs_.find(mod.name);
        knobs[mod.name] = found != modKnobs_.end() ? found->second : (mod.enabled ? 1.0F : 0.0F);
    }
    modKnobs_ = std::move(knobs);
}

Gdiplus::PointF MainPage::toContent(int x, int y) const
{
    return {(static_cast<float>(x) - offsetX_) / viewScale_, (static_cast<float>(y) - offsetY_) / viewScale_};
}

Gdiplus::RectF MainPage::controlBounds(int id) const
{
    switch (id) {
    case MinimizeButtonId: return Layout::captionButton(0, chromeWidth());
    case MaximizeButtonId: return Layout::captionButton(1, chromeWidth());
    case CloseButtonId: return Layout::captionButton(2, chromeWidth());
    case NavHomeId: return Layout::navButton(0);
    case NavVersionsId: return Layout::navButton(1);
    case NavNewsId: return Layout::navButton(2);
    case NavModsId: return Layout::navButton(3);
    case NavScreenshotsId: return Layout::navButton(4);
    case NavSkinsId: return Layout::navButton(5);
    case NavSettingsId: return Layout::navButton(6);
    case SkinFolderButtonId: return Layout::SkinsFolder;
    case SkinAddButtonId: return Layout::SkinsAdd;
    case SkinRemoveButtonId:
    case SkinReportButtonId:
        return Layout::SkinsRemove;
    case SkinShareButtonId: return Layout::SkinsShare;
    case SkinSaveButtonId: return Layout::SkinsReset;
    case SkinRefreshButtonId: return Layout::SkinsRefresh;
    case SkinSearchInputId: return Layout::SkinsSearch;
    case SkinSearchButtonId: return Layout::SkinsSearchGo;
    case SkinMineTabId: return Layout::skinsTab(0);
    case SkinExploreTabId: return Layout::skinsTab(1);
    case SkinClassicButtonId: return Layout::SkinsClassic;
    case SkinSlimButtonId: return Layout::SkinsSlim;
    case SkinUseButtonId:
    case SkinSignInButtonId:
        return Layout::SkinsUse;
    case SkinResetButtonId: return Layout::SkinsReset;
    case NavHelpId: return Layout::sidebarHelp(chromeHeight());
    case NicknameInputId: return Layout::NicknameEdit;
    case EditNicknameButtonId: return Layout::EditNickname;
    case OfflineButtonId: return Layout::OfflineMode;
    case OriginalButtonId: return Layout::OriginalMode;
    case VersionEternalButtonId: return Layout::versionAction(0);
    case VersionVanillaButtonId: return Layout::versionAction(1);
    case VersionChangeButtonId: return Layout::VersionRow;
    case UpdateDownloadButtonId:
    case RetryServerButtonId:
        return Layout::UpdateLink;
    case PlayButtonId: return Layout::Play;
    case PlayMenuButtonId: return Layout::PlayMenu;
    case QuickFolderButtonId: return Layout::quickAction(0);
    case QuickRepairButtonId: return Layout::quickAction(1);
    case QuickLogsButtonId: return Layout::quickAction(2);
    case SpotlightReadButtonId: return Layout::SpotlightRead;
    case CelebrationButtonId: return Layout::CelebrationButton;
    case LoginUserInputId:
    case LoginPasswordInputId:
    case LoginConfirmInputId:
    {
        const int row = Layout::loginRow(loginMode_, id == LoginUserInputId ? 0 : id == LoginPasswordInputId ? 1 : 2);
        return row < 0 ? Gdiplus::RectF{} : Layout::loginField(row);
    }
    case LoginSubmitButtonId: return Layout::LoginSubmit;
    case LoginRegisterButtonId: return Layout::LoginRegister;
    case LoginCloseButtonId: return Layout::LoginClose;
    case AccountSignOutButtonId: return Layout::EditNickname;
    case LevelsButtonId: return Layout::stat(1);
    case LevelsCloseButtonId: return Layout::LevelsClose;
    case ModpackChangesOkButtonId: return Layout::ModpackChangesOk;
    case ModpackChangesModsButtonId: return Layout::ModpackChangesMods;
    case SpotlightDismissButtonId: return Layout::SpotlightDismiss;
    case ScreenshotsRefreshButtonId: return Layout::ScreenshotsRefresh;
    case ScreenshotsFolderButtonId: return Layout::ScreenshotsFolder;
    case ViewerBackButtonId: return Layout::ViewerBack;
    case ViewerPreviousButtonId: return Layout::ViewerPrevious;
    case ViewerNextButtonId: return Layout::ViewerNext;
    case ViewerCopyButtonId: return galleryTab_ ? Layout::ViewerGroupCopy : Layout::ViewerCopy;
    case ViewerFolderButtonId: return Layout::ViewerFolder;
    case ViewerOpenButtonId: return Layout::ViewerOpen;
    case ViewerDeleteButtonId: return galleryTab_ ? Layout::ViewerGroupAction : Layout::ViewerDelete;
    case ViewerShareButtonId: return Layout::ViewerShare;
    case ViewerReportButtonId: return Layout::ViewerGroupAction;
    case ScreenshotsMineTabId: return Layout::screenshotsTab(0);
    case ScreenshotsGroupTabId: return Layout::screenshotsTab(1);
    case NewsAllButtonId: return {Layout::NewsAll.X + 20, Layout::NewsAll.Y, Layout::NewsAll.Width - 20, Layout::NewsAll.Height};
    case InstallationLogButtonId: return Layout::InstallationLog;
    case CancelInstallationButtonId: return Layout::InstallationCancel;
    case InstallationCloseButtonId: return Layout::InstallationClose;
    case InstallationRetryButtonId: return Layout::InstallationRetry;
    case ModsRefreshButtonId: return Layout::ModsRefresh;
    case ModsFolderButtonId: return Layout::ModsFolder;
    case ModsAddButtonId: return Layout::ModsAdd;
    case HelpRepairButtonId: return Layout::helpButton(0, 0);
    case HelpInstallLogButtonId: return Layout::helpButton(1, 0);
    case HelpForgeLogButtonId: return Layout::helpButton(1, 1);
    case HelpGameLogButtonId: return Layout::helpButton(1, 2);
    case HelpLogsFolderButtonId: return Layout::helpButton(1, 3);
    case HelpCopySupportButtonId: return Layout::helpButton(2, 0);
    case HelpReportIssueButtonId: return Layout::helpButton(2, 1);
    case HelpDataFolderButtonId: return Layout::helpButton(3, 0);
    default: break;
    }
    if (inRange(id, NewsCardFirstId, NewsCardCount)) return Layout::newsCard(id - NewsCardFirstId);
    if (inRange(id, NewsLinkFirstId, NewsLinkCount)) return Layout::newsLink(id - NewsLinkFirstId);
    if (inRange(id, ScreenshotCardFirstId, ScreenshotCardCount)) return Layout::screenshotCard(id - ScreenshotCardFirstId);
    if (inRange(id, SkinCardFirstId, SkinCardCount)) return Layout::skinCard(id - SkinCardFirstId);
    return {};
}

bool MainPage::controlVisible(int id) const
{
    if (chromeControl(id)) return true;
    const bool home = page_ == LauncherPage::Home;
    // A screen over Home (installation, then sign-in, then levels) leaves only its own controls.
    if (home && (installation_.visible || loginOpen_ || levelsOpen_)) {
        static constexpr int InstallationScreen[] = {InstallationLogButtonId, CancelInstallationButtonId, InstallationCloseButtonId,
                                                     InstallationRetryButtonId};
        static constexpr int LoginScreen[] = {LoginUserInputId, LoginPasswordInputId, LoginConfirmInputId, LoginSubmitButtonId,
                                              LoginRegisterButtonId, LoginCloseButtonId};
        const auto member = [id](const auto& ids) { return std::find(std::begin(ids), std::end(ids), id) != std::end(ids); };
        if (installation_.visible ? !member(InstallationScreen) : loginOpen_ ? !member(LoginScreen) : id != LevelsCloseButtonId)
            return false;
    }
    const bool leftColumn = home;
    // The modpack changelog takes the landscape's slot, with its own buttons.
    // The levels panel covers both while it is open.
    const bool heroVisible = home && !installation_.visible && !modpackChangesVisible_ && !levelsOpen_ && !loginOpen_;
    switch (id) {
    case NicknameInputId:
    case EditNicknameButtonId:
        return false;  // nickname mode is gone: the Conta Eternal replaced it
    case AccountSignOutButtonId:
        return home && !originalMode_ && eternalSignedIn();
    case SkinMineTabId:
    case SkinExploreTabId:
    case SkinClassicButtonId:
    case SkinSlimButtonId:
        return page_ == LauncherPage::Skins;
    case SkinFolderButtonId:
    case SkinAddButtonId:
        return page_ == LauncherPage::Skins && !skinExplore_;
    case SkinRefreshButtonId:
    case SkinSaveButtonId:
    case SkinSearchInputId:
    case SkinSearchButtonId:
        return page_ == LauncherPage::Skins && skinExplore_;
    case SkinUseButtonId:
        return page_ == LauncherPage::Skins && eternalSignedIn();
    case SkinResetButtonId:
        return page_ == LauncherPage::Skins && eternalSignedIn() && !skinExplore_;
    case SkinSignInButtonId:
        return page_ == LauncherPage::Skins && !eternalSignedIn();
    case SkinShareButtonId:
        return page_ == LauncherPage::Skins && !skinExplore_ && skinSelected_ > 0 &&
            static_cast<std::size_t>(skinSelected_) <= skinLibrary_.size() &&
            !skinsShared_.bySource(librarySourceKey(static_cast<std::size_t>(skinSelected_ - 1)));
    case SkinRemoveButtonId:
        if (skinExplore_) {
            const auto* skin = exploreSkin(exploreSelected_);
            return page_ == LauncherPage::Skins && skin && !skin->group && !skin->searched && skinsShared_.byId(skin->id);
        }
        return page_ == LauncherPage::Skins && skinSelected_ > 0 && static_cast<std::size_t>(skinSelected_) <= skinLibrary_.size();
    case SkinReportButtonId:
    {
        const auto* skin = skinExplore_ ? exploreSkin(exploreSelected_) : nullptr;
        return page_ == LauncherPage::Skins && skin && !skin->group && !skin->searched && !skinsShared_.byId(skin->id);
    }
    case LoginUserInputId:
    case LoginPasswordInputId:
    case LoginConfirmInputId:
        return home && !installation_.visible && loginOpen_ &&
            Layout::loginRow(loginMode_, id == LoginUserInputId ? 0 : id == LoginPasswordInputId ? 1 : 2) >= 0;
    case LoginSubmitButtonId:
    case LoginRegisterButtonId:
    case LoginCloseButtonId:
        return home && !installation_.visible && loginOpen_;
    case VersionEternalButtonId:
    case VersionVanillaButtonId:
        return page_ == LauncherPage::Versions;
    case OfflineButtonId:
    case OriginalButtonId:
    case VersionChangeButtonId:
    case PlayButtonId:
    case PlayMenuButtonId:
    case QuickFolderButtonId:
    case QuickRepairButtonId:
    case QuickLogsButtonId:
        return home;
    case UpdateDownloadButtonId:
        return home && !updateWarning_.empty() && viewState_.playEnabled;
    case RetryServerButtonId:
        return home && serverOffline_ && !vanillaSelected_ && updateWarning_.empty() && viewState_.playEnabled && !installation_.visible;
    case SpotlightReadButtonId:
    case SpotlightDismissButtonId:
        return heroVisible && spotlightIndex() >= 0 && celebration_ == 0;
    case CelebrationButtonId:
        return heroVisible && celebration_ > 0;
    case ModpackChangesOkButtonId:
    case ModpackChangesModsButtonId:
        return home && !installation_.visible && modpackChangesVisible_ && !levelsOpen_ && !loginOpen_;
    case LevelsButtonId:
        return home;
    case LevelsCloseButtonId:
        return home && !installation_.visible && levelsOpen_ && !loginOpen_;
    case ScreenshotsRefreshButtonId:
    case ScreenshotsMineTabId:
    case ScreenshotsGroupTabId:
        return page_ == LauncherPage::Screenshots && screenshotViewer_ < 0;
    case ScreenshotsFolderButtonId:
        return page_ == LauncherPage::Screenshots && screenshotViewer_ < 0 && !galleryTab_;
    case ViewerBackButtonId:
    case ViewerCopyButtonId:
        return page_ == LauncherPage::Screenshots && (currentScreenshot() || currentGalleryPhoto());
    case ViewerShareButtonId:
    case ViewerFolderButtonId:
    case ViewerOpenButtonId:
        return page_ == LauncherPage::Screenshots && currentScreenshot();
    case ViewerDeleteButtonId:
        // A group picture can only be removed from the computer that shared it.
        if (const auto* photo = currentGalleryPhoto()) return page_ == LauncherPage::Screenshots && galleryShared_.byId(photo->id);
        return page_ == LauncherPage::Screenshots && currentScreenshot();
    case ViewerReportButtonId:
        if (const auto* photo = currentGalleryPhoto()) return page_ == LauncherPage::Screenshots && !galleryShared_.byId(photo->id);
        return false;
    case ViewerPreviousButtonId:
        return page_ == LauncherPage::Screenshots && screenshotViewer_ > 0 && static_cast<std::size_t>(screenshotViewer_) < shotCount();
    case ViewerNextButtonId:
        return page_ == LauncherPage::Screenshots && screenshotViewer_ >= 0 && static_cast<std::size_t>(screenshotViewer_) + 1 < shotCount();
    case NewsAllButtonId:
        return leftColumn;
    case InstallationLogButtonId:
        return home && installation_.visible;
    case CancelInstallationButtonId:
        return home && installation_.visible && installation_.running;
    case InstallationCloseButtonId:
    case InstallationRetryButtonId:
        return home && installation_.visible && !installation_.running;
    case ModsRefreshButtonId:
    case ModsFolderButtonId:
    case ModsAddButtonId:
        return page_ == LauncherPage::Mods;
    default:
        break;
    }
    if (inRange(id, NewsCardFirstId, NewsCardCount))
        return leftColumn && static_cast<std::size_t>(id - NewsCardFirstId) < viewState_.news.size();
    if (inRange(id, NewsLinkFirstId, NewsLinkCount)) {
        const auto index = newsFirst_ + static_cast<std::size_t>(id - NewsLinkFirstId);
        return page_ == LauncherPage::News && index < viewState_.news.size() && !viewState_.news[index].url.empty();
    }
    if (inRange(id, HelpRepairButtonId, 8)) return page_ == LauncherPage::Help;
    if (inRange(id, ScreenshotCardFirstId, ScreenshotCardCount)) {
        const auto index = screenshotRow_ * Layout::ScreenshotColumns + static_cast<std::size_t>(id - ScreenshotCardFirstId);
        return page_ == LauncherPage::Screenshots && screenshotViewer_ < 0 && index < shotCount();
    }
    if (inRange(id, SkinCardFirstId, SkinCardCount)) {
        if (skinExplore_)
            return page_ == LauncherPage::Skins && exploreRow_ * Layout::SkinColumns + static_cast<std::size_t>(id - SkinCardFirstId) < exploreCount();
        const auto index = skinRow_ * Layout::SkinColumns + static_cast<std::size_t>(id - SkinCardFirstId);
        return page_ == LauncherPage::Skins && index < 1 + skinLibrary_.size();
    }
    return false;
}

bool MainPage::controlEnabled(int id) const
{
    const bool idle = viewState_.playEnabled && !updatingLauncher_;
    switch (id) {
    case NicknameInputId:
    case EditNicknameButtonId:
    case OfflineButtonId:
    case OriginalButtonId:
    case VersionEternalButtonId:
    case VersionVanillaButtonId:
    case PlayButtonId:
        return idle;
    case PlayMenuButtonId:
        return !updatingLauncher_;
    case QuickRepairButtonId:
    case HelpRepairButtonId:
        return canRepair();
    case CancelInstallationButtonId:
        return installation_.running && !installation_.cancelling;
    case ModsAddButtonId:
        return !gameRunning() && !installation_.running;
    case LoginUserInputId:
    case LoginPasswordInputId:
    case LoginConfirmInputId:
    case LoginSubmitButtonId:
    case LoginRegisterButtonId:
        return !loginBusy_;
    case AccountSignOutButtonId:
        return !gameRunning() && !installation_.running && !eternalPreparing_;
    case SkinUseButtonId:
        if (skinExplore_) return !skinBusy_ && exploreSkin(exploreSelected_);
        return !skinBusy_ && (skinSelected_ == 0 ? currentSkin_.has_value() : static_cast<std::size_t>(skinSelected_) <= skinLibrary_.size());
    case SkinResetButtonId:
    case SkinAddButtonId:
        return !skinBusy_;
    case SkinSaveButtonId:
        return exploreSkin(exploreSelected_) != nullptr;
    case SkinRefreshButtonId:
        return !exploreLoading_;
    case SkinSearchButtonId:
        return !skinSearching_;
    case SkinShareButtonId:
    case SkinReportButtonId:
        return !skinSharing_;
    case SkinRemoveButtonId:
        return !skinExplore_ || !skinSharing_;
    case ViewerShareButtonId:
        if (const auto* shot = currentScreenshot())
            return !galleryBusy_ && !galleryShared_.bySource(GallerySharedStore::sourceKey(shot->path, shot->size));
        return false;
    case ViewerDeleteButtonId:
    case ViewerReportButtonId:
        return !galleryBusy_;
    default:
        return true;
    }
}

std::wstring MainPage::controlLabel(int id) const
{
    switch (id) {
    case PlayButtonId:
        if (vanillaSelected_ && viewState_.playEnabled) return L"Jogar Minecraft 1.8.9";
        return viewState_.primaryActionLabel.empty() ? L"Jogar" : viewState_.primaryActionLabel;
    case VersionEternalButtonId: return vanillaSelected_ ? L"Usar o Modpack Eternal" : L"Jogar o Modpack Eternal";
    case VersionVanillaButtonId: return vanillaSelected_ ? L"Jogar Minecraft 1.8.9" : L"Usar Minecraft 1.8.9";
    case VersionChangeButtonId: return vanillaSelected_ ? L"Trocar versão, selecionada Minecraft 1.8.9" : L"Trocar versão, selecionado Modpack Eternal";
    case OfflineButtonId: return originalMode_ ? L"Nickname" : L"Nickname (selecionado)";
    case OriginalButtonId: return originalMode_ ? L"Conta original (selecionado)" : L"Conta original";
    case MaximizeButtonId: return window_ && IsZoomed(window_) ? L"Restaurar" : L"Maximizar";
    case CancelInstallationButtonId: return installation_.cancelling ? L"Cancelando instalação" : L"Cancelar instalação";
    case InstallationRetryButtonId:
        return installation_.result && installation_.result->outcome == InstallationOutcome::Cancelled ? L"Continuar a instalação" : L"Tentar de novo";
    case InstallationCloseButtonId: return L"Fechar e voltar ao início";
    default: break;
    }
    if (inRange(id, NavHomeId, NavigationCount)) {
        // Indexed by id: Home, News, Mods, Settings, Help, Versions, Screenshots, Skins.
        const wchar_t* names[] = {L"Início", L"Notícias", L"Mods", L"Configurações", L"Ajuda", L"Versões", L"Capturas", L"Skins"};
        const LauncherPage pages[] = {LauncherPage::Home, LauncherPage::News, LauncherPage::Mods, LauncherPage::Settings, LauncherPage::Help,
            LauncherPage::Versions, LauncherPage::Screenshots, LauncherPage::Skins};
        const auto index = static_cast<std::size_t>(id - NavHomeId);
        std::wstring label = names[index];
        if (id == NavNewsId) {
            const int unread = unreadNewsCount();
            if (unread > 0) label += L", " + std::to_wstring(unread) + (unread == 1 ? L" novidade" : L" novidades");
        }
        return label + (page_ == pages[index] ? L" (página atual)" : L"");
    }
    if (inRange(id, NewsCardFirstId, NewsCardCount)) {
        const auto index = static_cast<std::size_t>(id - NewsCardFirstId);
        if (index >= viewState_.news.size()) return {};
        return (viewState_.news[index].unread ? L"Nova: " : L"") + viewState_.news[index].title;
    }
    if (id == SpotlightReadButtonId) {
        const int spotlight = spotlightIndex();
        return spotlight >= 0 ? L"Ler novidade: " + viewState_.news[static_cast<std::size_t>(spotlight)].title : std::wstring(L"Ler novidade");
    }
    if (inRange(id, ScreenshotCardFirstId, ScreenshotCardCount)) {
        const auto index = screenshotRow_ * Layout::ScreenshotColumns + static_cast<std::size_t>(id - ScreenshotCardFirstId);
        if (galleryTab_)
            return index < galleryPhotos_.size() ? L"Captura de " + galleryPhotos_[index].name : std::wstring{};
        return index < screenshots_.size() ? L"Captura de " + ScreenshotLibrary::formatDate(screenshots_[index].modified) : std::wstring{};
    }
    if (inRange(id, SkinCardFirstId, SkinCardCount) && skinExplore_) {
        const auto* skin = exploreSkin(static_cast<int>(exploreRow_ * Layout::SkinColumns) + id - SkinCardFirstId);
        if (!skin) return {};
        if (skin->searched) return L"Skin de " + skin->title + L", encontrada na busca";
        return skin->group ? L"Skin de " + skin->title + L", jogador do servidor" : L"Skin " + skin->title + L", compartilhada por " + skin->author;
    }
    if (id == SkinMineTabId) return skinExplore_ ? L"Minhas skins" : L"Minhas skins (selecionada)";
    if (id == SkinExploreTabId) return skinExplore_ ? L"Explorar skins (selecionada)" : L"Explorar skins";
    if (id == SkinRemoveButtonId && skinExplore_) return L"Remover skin compartilhada";
    if (inRange(id, SkinCardFirstId, SkinCardCount)) {
        const auto index = skinRow_ * Layout::SkinColumns + static_cast<std::size_t>(id - SkinCardFirstId);
        if (index == 0) return L"Sua skin atual";
        return index <= skinLibrary_.size() ? L"Skin " + skinLibrary_[index - 1].name : std::wstring{};
    }
    if (id == SkinClassicButtonId) return skinSlimChoice_ ? L"Braços clássicos" : L"Braços clássicos (selecionado)";
    if (id == SkinSlimButtonId) return skinSlimChoice_ ? L"Braços finos (selecionado)" : L"Braços finos";
    if (id == LevelsButtonId) {
        if (const auto seconds = serverSecondsForAccount())
            return L"Tempo jogado " + PlaytimeStore::formatDuration(*seconds) + L", nível " + RankTier::name(RankTier::level(*seconds)) +
                L". Ver todos os níveis";
        return L"Ver todos os níveis de tempo de jogo";
    }
    if (id == ScreenshotsMineTabId) return galleryTab_ ? L"Minhas capturas" : L"Minhas capturas (selecionada)";
    if (id == ScreenshotsGroupTabId) return galleryTab_ ? L"Galeria do grupo (selecionada)" : L"Galeria do grupo";
    if (id == ViewerDeleteButtonId && galleryTab_) return L"Remover da galeria do grupo";
    for (const auto& spec : ControlSpecs) if (spec.id == id && spec.label) return spec.label;
    return {};
}

void MainPage::layoutControls()
{
    RECT client{};
    GetClientRect(window_, &client);
    if (client.right <= 0 || client.bottom <= 0) return;
    clientWidth_ = static_cast<float>(client.right);
    clientHeight_ = static_cast<float>(client.bottom);
    viewScale_ = std::min(clientWidth_ / WindowWidth, clientHeight_ / WindowHeight);
    offsetX_ = std::floor((clientWidth_ - WindowWidth * viewScale_) / 2);
    offsetY_ = std::floor((clientHeight_ - WindowHeight * viewScale_) / 2);
    const float s = viewScale_;
    for (auto& control : controls_) {
        if (!control.window) continue;
        auto bounds = controlBounds(control.id);
        const float ox = control.chrome ? 0 : offsetX_, oy = control.chrome ? 0 : offsetY_;
        if (control.id == NicknameInputId) {
            // The single-line EDIT draws its text at the top; size it to one line of the display face.
            const float line = 30;
            bounds = {bounds.X, bounds.Y + (bounds.Height - line) / 2, bounds.Width, line};
        } else if ((control.id == LoginUserInputId || control.id == LoginPasswordInputId || control.id == LoginConfirmInputId) &&
                   bounds.Width > 0) {
            // Inside the drawn well, after its icon, one line tall.
            const float line = 20;
            bounds = {bounds.X + 40, bounds.Y + (bounds.Height - line) / 2, bounds.Width - 54, line};
        } else if (control.id == SkinSearchInputId) {
            // Between the magnifier and the go button.
            const float line = 20;
            bounds = {bounds.X + 40, bounds.Y + (bounds.Height - line) / 2, bounds.Width - 40 - 46, line};
        }
        const int x = static_cast<int>(std::floor(ox + bounds.X * s)), y = static_cast<int>(std::floor(oy + bounds.Y * s));
        const int right = static_cast<int>(std::ceil(ox + bounds.GetRight() * s));
        const int bottom = static_cast<int>(std::ceil(oy + bounds.GetBottom() * s));
        SetWindowPos(control.window, nullptr, x, y, right - x, bottom - y, SWP_NOZORDER | SWP_NOACTIVATE);
        control.rect = {x, y, right, bottom};
    }
    if (nicknameInput_) {
        const HFONT font = CreateFontW(-static_cast<int>(std::round(22 * s)), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            DrawingUtils::displayFontFamily());
        if (font) {
            SendMessage(nicknameInput_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (inputFont_) DeleteObject(inputFont_);
            inputFont_ = font;
        }
    }
    if (loginUser_ || loginPassword_) {
        const HFONT font = CreateFontW(-static_cast<int>(std::round(15 * s)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, Theme::FontFamily);
        if (font) {
            for (const HWND edit : {loginUser_, loginPassword_, loginConfirm_, skinSearch_}) if (edit) SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (loginFont_) DeleteObject(loginFont_);
            loginFont_ = font;
        }
    }
    settingsPage_.layout(viewScale_, offsetX_, offsetY_);
    navIndicator_ = navigationTarget();
}

void MainPage::syncControls()
{
    // Hiding the focused child hands focus to the parent; remember where it was.
    const HWND focused = GetFocus();
    for (auto& control : controls_) {
        if (!control.window) continue;
        const bool visible = controlVisible(control.id);
        if (((GetWindowLongPtrW(control.window, GWL_STYLE) & WS_VISIBLE) != 0) != visible)
            ShowWindow(control.window, visible ? SW_SHOWNA : SW_HIDE);
        const bool enabled = controlEnabled(control.id);
        if ((IsWindowEnabled(control.window) != FALSE) != enabled) EnableWindow(control.window, enabled);
        if (!enabled || !visible) control.hovered = false;
        // Text fields keep what the player typed; only buttons take their label from here.
        if (control.id == NicknameInputId || control.id == LoginUserInputId || control.id == LoginPasswordInputId ||
            control.id == LoginConfirmInputId || control.id == SkinSearchInputId) continue;
        const auto label = controlLabel(control.id);
        std::array<wchar_t, 256> current{};
        GetWindowTextW(control.window, current.data(), static_cast<int>(current.size()));
        if (label != current.data()) SetWindowTextW(control.window, label.c_str());
    }
    if (page_ == LauncherPage::Settings) settingsPage_.setLocked(!viewState_.playEnabled || gameRunning());
    // Never leave keyboard focus on a control that just disappeared.
    if (focused && focused != window_ && IsChild(window_, focused) && !(GetWindowLongPtrW(focused, GWL_STYLE) & WS_VISIBLE)) {
        const int navigation[] = {NavHomeId, NavVersionsId, NavNewsId, NavModsId, NavScreenshotsId, NavSkinsId, NavSettingsId, NavHelpId};
        if (const auto* control = findControl(navigation[static_cast<int>(page_)]); control && control->window)
            SetFocus(control->window);
    }
}

LRESULT CALLBACK MainPage::controlProcedure(HWND control, UINT message, WPARAM wParam, LPARAM lParam,
                                            UINT_PTR subclass, DWORD_PTR data)
{
    auto* page = reinterpret_cast<MainPage*>(data);
    const int id = static_cast<int>(subclass);
    // WM_DRAWITEM covers the complete button, including its rounded corners.
    if (message == WM_ERASEBKGND) return 1;
    auto* entry = page->findControl(id);
    if (id == PlayButtonId && IsWindowEnabled(control)) {
        if (message == WM_MOUSEMOVE) {
            RECT bounds{};
            GetClientRect(control, &bounds);
            const float light = bounds.right > 0 ? std::clamp(static_cast<float>(GET_X_LPARAM(lParam)) / static_cast<float>(bounds.right), 0.0F, 1.0F) : .5F;
            if (std::abs(page->playLightTarget_ - light) > .005F) { page->playLightTarget_ = light; page->redraw(true); }
            if (entry && !entry->hovered) page->playSweep_ = 0;
        }
        if (message == WM_LBUTTONDOWN || (message == WM_KEYDOWN && wParam == VK_SPACE && !(lParam & (1LL << 30))))
            page->playPulse_ = 1;
        if (message == WM_SETFOCUS) page->playSweep_ = 0;
    }
    if (message == WM_MOUSEMOVE && entry && !entry->hovered && IsWindowEnabled(control)) {
        entry->hovered = true;
        TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE, control, 0};
        TrackMouseEvent(&track);
        // Hover only changes this button's pixels; the easing marks it frame by frame.
        page->markControl(id);
        page->redraw(true);
    } else if (message == WM_MOUSELEAVE && entry) {
        entry->hovered = false;
        page->markControl(id);
        page->redraw(true);
    } else if (message == WM_SETCURSOR && IsWindowEnabled(control)) {
        const bool caption = id == MinimizeButtonId || id == MaximizeButtonId || id == CloseButtonId;
        SetCursor(LoadCursor(nullptr, caption ? IDC_ARROW : IDC_HAND));
        return TRUE;
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(control, controlProcedure, subclass);
    }
    const auto result = DefSubclassProc(control, message, wParam, lParam);
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
        message == WM_KEYDOWN || message == WM_KEYUP) {
        page->markControl(id);
        page->redraw(true);
    }
    return result;
}

bool MainPage::handleKeyboard(MSG& message)
{
    if (message.message == WM_KEYDOWN) {
        if (page_ == LauncherPage::Screenshots && screenshotViewer_ >= 0) {
            if (message.wParam == VK_ESCAPE) { closeScreenshot(); return true; }
            if (message.wParam == VK_LEFT && screenshotViewer_ > 0) { openScreenshot(screenshotViewer_ - 1); return true; }
            if (message.wParam == VK_RIGHT && static_cast<std::size_t>(screenshotViewer_) + 1 < shotCount()) {
                openScreenshot(screenshotViewer_ + 1);
                return true;
            }
        }
        if (message.wParam == VK_ESCAPE && page_ == LauncherPage::Home && loginOpen_) { closeLogin(); return true; }
        if (message.wParam == VK_RETURN && loginOpen_) {
            // Enter walks the visible fields in order and submits from the last one.
            // In "Entrar", Enter on the nickname submits right away: an empty password is how
            // a first access starts.
            const HWND focus = GetFocus();
            for (int field = 0; field < 3; ++field) {
                if (loginField(field) != focus || Layout::loginRow(loginMode_, field) < 0) continue;
                if (loginMode_ != 0)
                    for (int next = field + 1; next < 3; ++next)
                        if (Layout::loginRow(loginMode_, next) >= 0 && GetWindowTextLengthW(loginField(next)) == 0) {
                            SetFocus(loginField(next));
                            return true;
                        }
                submitLogin();
                return true;
            }
        }
        if (message.wParam == VK_RETURN && skinSearch_ && GetFocus() == skinSearch_) { searchSkin(); return true; }
        if (message.wParam == VK_ESCAPE && page_ == LauncherPage::Home && levelsOpen_) { showLevels(false); return true; }
        if (message.wParam == VK_ESCAPE && page_ == LauncherPage::Home && installation_.visible) {
            if (!installation_.running) closeInstallation();
            return true;
        }
        if (message.wParam == VK_ESCAPE && page_ != LauncherPage::Home) { showPage(LauncherPage::Home); return true; }
        if (message.wParam == VK_RETURN) {
            const HWND focus = GetFocus();
            if (page_ == LauncherPage::Settings) {
                const int id = settingsPage_.focusedControlId();
                if (id == SettingsPage::MinRam || id == SettingsPage::MaxRam || id == SettingsPage::Width ||
                    id == SettingsPage::Height || id == SettingsPage::JavaPath || id == SettingsPage::GamePath) {
                    saveSettings();
                    return true;
                }
                if (SettingsPage::containsId(id) && id != SettingsPage::RamSlider) {
                    SendMessage(GetDlgItem(window_, id), BM_CLICK, 0, 0);
                    return true;
                }
            }
            if (focus && focus == nicknameInput_) { handlePrimaryAction(); return true; }
            for (const auto& control : controls_)
                if (control.window && focus == control.window) { SendMessage(control.window, BM_CLICK, 0, 0); return true; }
        }
    }
    return IsDialogMessage(window_, &message) != FALSE;
}

// ---------------------------------------------------------------------------------------------
// Navigation and page actions
// ---------------------------------------------------------------------------------------------

void MainPage::showPage(LauncherPage page)
{
    if (page_ == page) return;
    const HWND focused = GetFocus();
    if (page_ == LauncherPage::Settings) settingsPage_.show(false);
    // Leaving the list: what was on it is read now, so the NOVO tags go away.
    if (page_ == LauncherPage::News)
        for (auto& item : viewState_.news) item.unread = false;
    if (page_ == LauncherPage::Screenshots) {
        screenshotViewer_ = -1;
        viewerImage_.reset(); viewerImagePath_.clear();
        galleryImage_.reset(); galleryImageId_.clear();
    }
    page_ = page;
    if (page == LauncherPage::Settings) {
        settingsPage_.open(settings_, settingsWarning_);
        settingsPage_.setLocked(!viewState_.playEnabled || gameRunning());
        settingsPage_.show(true);
    }
    if (page == LauncherPage::Mods) { refreshMods(); modsMessage_.clear(); modsMessageError_ = false; }
    if (page == LauncherPage::News) markNewsRead();
    if (page == LauncherPage::Skins) {
        skinsMessage_.clear();
        skinsMessageError_ = false;
        refreshSkinLibrary();
        fetchCurrentSkin();
        if (skinExplore_) loadExplore();
    }
    if (page == LauncherPage::Screenshots) {
        screenshotsMessage_.clear();
        screenshotsMessageError_ = false;
        if (galleryTab_) { galleryLoaded_ = false; galleryUnavailable_ = false; }
        refreshScreenshots();
    }
    if (page == LauncherPage::Help) helpMessage_.clear();
    if (page == LauncherPage::Versions) vanillaInstalled_ = VanillaMinecraftService::isInstalled();
    portalHovered_ = false;
    modsHoverRow_ = modsHoverAction_ = -1;
    pageFade_ = motionEnabled_ ? 0.0F : 1.0F;
    redraw();
    // Keyboard focus follows the page: a control of the previous page never keeps it.
    if (focused && focused != window_ && IsChild(window_, focused) && !(GetWindowLongPtrW(focused, GWL_STYLE) & WS_VISIBLE)) {
        const int navigation[] = {NavHomeId, NavVersionsId, NavNewsId, NavModsId, NavScreenshotsId, NavSkinsId, NavSettingsId, NavHelpId};
        if (const auto* control = findControl(navigation[static_cast<int>(page_)]); control && control->window)
            SetFocus(control->window);
    }
}

void MainPage::handleCommand(int id, int)
{
    if (inRange(id, NewsCardFirstId, NewsCardCount)) {
        const auto count = viewState_.news.size();
        const auto last = count > NewsView::Visible ? count - NewsView::Visible : 0;
        newsFirst_ = std::min(static_cast<std::size_t>(id - NewsCardFirstId), last);
        showPage(LauncherPage::News);
        return;
    }
    if (inRange(id, NewsLinkFirstId, NewsLinkCount)) {
        const auto index = newsFirst_ + static_cast<std::size_t>(id - NewsLinkFirstId);
        if (index < viewState_.news.size() && !viewState_.news[index].url.empty())
            ShellExecuteW(window_, L"open", viewState_.news[index].url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (inRange(id, ScreenshotCardFirstId, ScreenshotCardCount)) {
        openScreenshot(static_cast<int>(screenshotRow_ * Layout::ScreenshotColumns) + id - ScreenshotCardFirstId);
        return;
    }
    if (inRange(id, SkinCardFirstId, SkinCardCount)) {
        if (skinExplore_) selectExplore(static_cast<int>(exploreRow_ * Layout::SkinColumns) + id - SkinCardFirstId);
        else selectSkin(static_cast<int>(skinRow_ * Layout::SkinColumns) + id - SkinCardFirstId);
        return;
    }
    const auto logs = gameDirectory() / L"logs";
    switch (id) {
    case MinimizeButtonId: PostMessageW(window_, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
    case MaximizeButtonId: PostMessageW(window_, WM_SYSCOMMAND, IsZoomed(window_) ? SC_RESTORE : SC_MAXIMIZE, 0); break;
    case CloseButtonId: PostMessageW(window_, WM_CLOSE, 0, 0); break;
    case NavHomeId: showPage(LauncherPage::Home); break;
    case NavVersionsId:
    case VersionChangeButtonId:
        showPage(LauncherPage::Versions);
        break;
    case NavNewsId: showPage(LauncherPage::News); break;
    case NavModsId: showPage(LauncherPage::Mods); break;
    case NavScreenshotsId:
        if (page_ == LauncherPage::Screenshots) closeScreenshot();
        else showPage(LauncherPage::Screenshots);
        break;
    case NavSkinsId: showPage(LauncherPage::Skins); break;
    case SkinFolderButtonId: openPath(skinLibraryDirectory(), true); break;
    case SkinAddButtonId: addSkin(); break;
    case SkinRemoveButtonId:
        if (skinExplore_) removeCommunitySkin();
        else removeSkin();
        break;
    case SkinMineTabId: showSkinsTab(false); break;
    case SkinExploreTabId: showSkinsTab(true); break;
    case SkinRefreshButtonId: exploreLoaded_ = false; loadExplore(); break;
    case SkinShareButtonId: shareLibrarySkin(); break;
    case SkinReportButtonId: reportCommunitySkin(); break;
    case SkinSaveButtonId: saveExploreSkin(); break;
    case SkinSearchButtonId: searchSkin(); break;
    case SkinClassicButtonId: skinSlimChoice_ = false; redraw(); break;
    case SkinSlimButtonId: skinSlimChoice_ = true; redraw(); break;
    case SkinUseButtonId: useSelectedSkin(); break;
    case SkinResetButtonId: resetSkin(); break;
    case SkinSignInButtonId: openLogin(false); break;
    case NavSettingsId: showPage(LauncherPage::Settings); break;
    case NavHelpId: showPage(LauncherPage::Help); break;
    case PlayButtonId: handlePrimaryAction(); break;
    case PlayMenuButtonId: showPlayMenu(); break;
    case OfflineButtonId:
        if (viewState_.playEnabled) { originalMode_ = false; startSkinLookup(); redraw(); }
        break;
    case OriginalButtonId:
        if (viewState_.playEnabled) { originalMode_ = true; startSkinLookup(); redraw(); }
        break;
    case VersionEternalButtonId:
    case VersionVanillaButtonId:
        // The selected card's button plays; the other one only selects.
        if (viewState_.playEnabled) {
            const bool vanilla = id == VersionVanillaButtonId;
            if (vanillaSelected_ == vanilla) { showPage(LauncherPage::Home); handlePrimaryAction(); }
            else { vanillaSelected_ = vanilla; redraw(); }
        }
        break;
    case EditNicknameButtonId:
        SetFocus(nicknameInput_);
        SendMessageW(nicknameInput_, EM_SETSEL, 0, -1);
        break;
    case UpdateDownloadButtonId:
        ShellExecuteW(window_, L"open", std::wstring(LauncherConfig::LauncherDownloadUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case QuickFolderButtonId:
    case MenuOpenGameFolder:
        openPath(gameDirectory(), true);
        break;
    case MenuOpenModsFolder:
    case ModsFolderButtonId:
        openPath(gameDirectory() / L"mods", true);
        break;
    case QuickRepairButtonId:
    case MenuRepair:
    case HelpRepairButtonId:
        startRepair();
        break;
    case QuickLogsButtonId:
    case MenuOpenLogs:
    case HelpLogsFolderButtonId:
        openPath(logs, true);
        break;
    case SpotlightReadButtonId:
    {
        // Open the list at the announced item.
        const int spotlight = spotlightIndex();
        const auto count = viewState_.news.size();
        const auto last = count > NewsView::Visible ? count - NewsView::Visible : 0;
        newsFirst_ = spotlight >= 0 ? std::min(static_cast<std::size_t>(spotlight), last) : 0;
        showPage(LauncherPage::News);
        break;
    }
    case SpotlightDismissButtonId:
        dismissSpotlight();
        break;
    case CelebrationButtonId:
        dismissCelebration();
        break;
    case LoginSubmitButtonId: submitLogin(); break;
    case LoginCloseButtonId: closeLogin(); break;
    case LoginRegisterButtonId:
        // "Criar conta" from sign-in; "Voltar" / "Já tenho conta" from the other two.
        setLoginMode(loginMode_ == 0 ? 2 : 0);
        break;
    case AccountSignOutButtonId: signOutEternal(); break;
    case LevelsButtonId: showLevels(!levelsOpen_); break;
    case LevelsCloseButtonId: showLevels(false); break;
    case ModpackChangesOkButtonId:
        acknowledgeModpackChanges();
        if (const auto* play = findControl(PlayButtonId); play && play->window) SetFocus(play->window);
        break;
    case ModpackChangesModsButtonId:
        acknowledgeModpackChanges();
        showPage(LauncherPage::Mods);
        break;
    case NewsAllButtonId:
        newsFirst_ = 0;
        showPage(LauncherPage::News);
        break;
    case ScreenshotsRefreshButtonId:
        if (galleryTab_) {
            galleryLoaded_ = false;
            galleryUnavailable_ = false;
            screenshotsMessage_.clear();
            requestGalleryImages();
            redraw();
            break;
        }
        refreshScreenshots();
        setScreenshotsMessage(screenshots_.empty() ? L"Nenhuma captura encontrada." : L"Lista atualizada.");
        break;
    case ScreenshotsMineTabId: showGalleryTab(false); break;
    case ScreenshotsGroupTabId: showGalleryTab(true); break;
    case ViewerShareButtonId: shareScreenshot(); break;
    case ViewerReportButtonId: reportGalleryPhoto(); break;
    case ScreenshotsFolderButtonId:
        openPath(gameDirectory() / L"screenshots", true);
        break;
    case ViewerBackButtonId: closeScreenshot(); break;
    case ViewerPreviousButtonId: if (screenshotViewer_ > 0) openScreenshot(screenshotViewer_ - 1); break;
    case ViewerNextButtonId: openScreenshot(screenshotViewer_ + 1); break;
    case ViewerCopyButtonId: copyCurrentPicture(); break;
    case ViewerFolderButtonId:
        if (const auto* shot = currentScreenshot()) {
            // Explorer opens the folder with the file selected.
            const std::wstring arguments = L"/select,\"" + shot->path.wstring() + L"\"";
            ShellExecuteW(window_, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
        }
        break;
    case ViewerOpenButtonId:
        if (const auto* shot = currentScreenshot()) openPath(shot->path);
        break;
    case ViewerDeleteButtonId:
        if (galleryTab_) removeGalleryPhoto();
        else deleteScreenshot();
        break;
    case CancelInstallationButtonId: cancelInstallation(); break;
    case InstallationCloseButtonId: closeInstallation(); break;
    case InstallationRetryButtonId:
        if (!installation_.running) handlePrimaryAction();
        break;
    case RetryServerButtonId: retryServer(); break;
    case InstallationLogButtonId:
    case HelpInstallLogButtonId:
        openLog(logs / L"installation.log");
        break;
    case HelpForgeLogButtonId: openLog(logs / L"forge-installer.log"); break;
    case HelpGameLogButtonId: openLog(logs / L"latest.log"); break;
    case HelpCopySupportButtonId: copySupportInformation(); break;
    case HelpReportIssueButtonId:
        ShellExecuteW(window_, L"open", IssueUrl, nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case HelpDataFolderButtonId: openPath(LauncherSettingsStore::dataDirectory(), true); break;
    case ModsRefreshButtonId:
        refreshMods();
        modsMessage_ = L"Lista atualizada.";
        modsMessageError_ = false;
        redraw();
        break;
    case ModsAddButtonId: addMod(); break;
    case MenuTrayOpen: restoreFromTray(); break;
    case MenuTrayExit:
        forceExit_ = true;
        PostMessageW(window_, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

void MainPage::showPlayMenu()
{
    const auto* button = findControl(PlayMenuButtonId);
    if (!button || !button->window) return;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, MenuOpenGameFolder, L"Abrir pasta do jogo");
    AppendMenuW(menu, MF_STRING, MenuOpenModsFolder, L"Abrir pasta de mods");
    AppendMenuW(menu, MF_STRING, MenuOpenLogs, L"Abrir logs");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (canRepair() ? 0U : MF_GRAYED), MenuRepair, L"Reparar instalação");
    RECT bounds{};
    GetWindowRect(button->window, &bounds);
    TrackPopupMenuEx(menu, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, bounds.right, bounds.bottom + 4, window_, nullptr);
    DestroyMenu(menu);
}

void MainPage::scrollContent(int delta, Gdiplus::PointF point)
{
    const int notches = delta / WHEEL_DELTA != 0 ? delta / WHEEL_DELTA : (delta > 0 ? 1 : -1);
    if (page_ == LauncherPage::Home) {
        if (!Layout::rankingCard().Contains(point.X, point.Y) || rankingState_ != RankingState::Ready) return;
        // Two rows per wheel notch.
        const float maximum = HomeView::maximumRankingScroll(leaderboard_.players.size());
        const float target = std::clamp(rankingScroll_ - static_cast<float>(notches) * 2 * HomeView::RankingRowHeight, 0.0F, maximum);
        if (target != rankingScroll_) { rankingScroll_ = target; redraw(); }
    } else if (page_ == LauncherPage::News) {
        const auto count = viewState_.news.size();
        const auto last = static_cast<int>(count > NewsView::Visible ? count - NewsView::Visible : 0);
        const int first = std::clamp(static_cast<int>(newsFirst_) - notches, 0, last);
        if (static_cast<std::size_t>(first) != newsFirst_) { newsFirst_ = static_cast<std::size_t>(first); redraw(); }
    } else if (page_ == LauncherPage::Skins) {
        if (!Layout::SkinsLibrary.Contains(point.X, point.Y)) return;
        auto& current = skinExplore_ ? exploreRow_ : skinRow_;
        const int last = static_cast<int>(SkinsView::lastFirstRow(skinExplore_ ? exploreCount() : 1 + skinLibrary_.size()));
        const int row = std::clamp(static_cast<int>(current) - notches, 0, last);
        if (static_cast<std::size_t>(row) != current) { current = static_cast<std::size_t>(row); redraw(); }
    } else if (page_ == LauncherPage::Screenshots) {
        if (screenshotViewer_ >= 0) {
            // In the viewer the wheel steps through the pictures.
            const int target = std::clamp(screenshotViewer_ - notches, 0, static_cast<int>(shotCount()) - 1);
            if (target != screenshotViewer_) openScreenshot(target);
            return;
        }
        const int last = static_cast<int>(ScreenshotsView::lastFirstRow(shotCount()));
        const int row = std::clamp(static_cast<int>(screenshotRow_) - notches, 0, last);
        if (static_cast<std::size_t>(row) != screenshotRow_) {
            screenshotRow_ = static_cast<std::size_t>(row);
            requestScreenshotImages();
            redraw();
        }
    } else if (page_ == LauncherPage::Mods) {
        const auto model = modsModel();
        const float step = static_cast<float>(notches) * 3 * ModsView::ServerRowHeight;
        if (Layout::ModsServer.Contains(point.X, point.Y))
            serverScroll_ = std::clamp(serverScroll_ - step, 0.0F, ModsView::maximumServerScroll(model));
        else if (Layout::ModsUser.Contains(point.X, point.Y))
            userScroll_ = std::clamp(userScroll_ - step, 0.0F, ModsView::maximumUserScroll(model));
        else return;
        const auto hit = ModsView::hitTest(modsModel(), point.X, point.Y);
        modsHoverRow_ = hit.row;
        modsHoverAction_ = hit.action == ModsView::Action::Toggle ? 0 : hit.action == ModsView::Action::Remove ? 1 : -1;
        redraw();
    }
}

void MainPage::handleModsClick(Gdiplus::PointF point)
{
    const auto model = modsModel();
    const auto hit = ModsView::hitTest(model, point.X, point.Y);
    if (hit.row < 0 || hit.action == ModsView::Action::None) return;
    if (model.locked) {
        modsMessage_ = L"Feche o jogo e espere a preparação terminar para mudar os mods.";
        modsMessageError_ = true;
        redraw();
        return;
    }
    const auto mod = mods_.user[static_cast<std::size_t>(hit.row)];
    std::wstring error;
    if (hit.action == ModsView::Action::Toggle) {
        if (ModInventory::setEnabled(mod, !mod.enabled, error)) {
            modsMessage_ = mod.name + (mod.enabled ? L" desativado." : L" ativado.") + L" Vale a partir do próximo Jogar.";
            modsMessageError_ = false;
        } else { modsMessage_ = error; modsMessageError_ = true; }
    } else {
        const auto prompt = L"Mover \"" + mod.name + L"\" para a Lixeira?\n\nVocê pode recuperá-lo de lá se mudar de ideia.";
        if (MessageBoxW(window_, prompt.c_str(), L"Remover mod", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) != IDOK) return;
        if (ModInventory::remove(mod, error)) { modsMessage_ = mod.name + L" foi para a Lixeira."; modsMessageError_ = false; }
        else { modsMessage_ = error; modsMessageError_ = true; }
    }
    refreshMods();
    redraw();
}

void MainPage::refreshMods()
{
    mods_ = ModInventory::scan(gameDirectory() / L"mods", fileServerManifest_);
    modsScanned_ = true;
    syncModKnobs();
    const auto model = modsModel();
    serverScroll_ = std::min(serverScroll_, ModsView::maximumServerScroll(model));
    userScroll_ = std::min(userScroll_, ModsView::maximumUserScroll(model));
}

void MainPage::addMod()
{
    std::array<wchar_t, 32768> file{};
    OPENFILENAMEW dialog{sizeof(OPENFILENAMEW)};
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"Mods do Minecraft (*.jar)\0*.jar\0\0";
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrTitle = L"Adicionar mod";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return;
    std::wstring error;
    const std::filesystem::path source(file.data());
    if (ModInventory::add(gameDirectory() / L"mods", source, error)) {
        modsMessage_ = source.filename().wstring() + L" adicionado. Vale a partir do próximo Jogar.";
        modsMessageError_ = false;
    } else { modsMessage_ = error; modsMessageError_ = true; }
    refreshMods();
    redraw();
}

std::filesystem::path MainPage::gameDirectory() const
{
    return LauncherSettingsStore::effectiveGameDirectory(settings_);
}

bool MainPage::openPath(const std::filesystem::path& path, bool createDirectory)
{
    std::error_code ignored;
    if (createDirectory) std::filesystem::create_directories(path, ignored);
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result > 32) return true;
    const std::wstring message = L"Não foi possível abrir " + path.wstring() + L".";
    if (page_ == LauncherPage::Help) setHelpMessage(message, true);
    else MessageBoxW(window_, message.c_str(), L"Eternal Launcher", MB_OK | MB_ICONWARNING);
    return false;
}

void MainPage::openLog(const std::filesystem::path& file)
{
    std::error_code ignored;
    if (std::filesystem::is_regular_file(file, ignored)) { openPath(file); return; }
    // No log yet: open the folder and say why.
    if (page_ == LauncherPage::Help)
        setHelpMessage(file.filename().wstring() + L" ainda não existe. Abrimos a pasta de logs.", false);
    openPath(file.parent_path(), true);
}

void MainPage::setHelpMessage(std::wstring message, bool error)
{
    helpMessage_ = std::move(message);
    helpMessageError_ = error;
    redraw();
}

void MainPage::copySupportInformation()
{
    std::wostringstream text;
    text << L"Eternal Launcher " << LauncherConfig::LauncherVersion << L"\r\n";
    text << L"Minecraft " << LauncherConfig::MinecraftVersion << L" · Forge " << LauncherConfig::ForgeVersion << L"\r\n";
    const auto product = registryText(L"ProductName"), display = registryText(L"DisplayVersion"), build = registryText(L"CurrentBuild");
    if (!product.empty()) text << product << L" " << display << L" (build " << build << L")\r\n";
    text << L"RAM do PC: " << LauncherSettingsStore::physicalMemoryMegabytes() << L" MiB · RAM do jogo: "
         << settings_.minimumMemoryMegabytes << L"–" << settings_.maximumMemoryMegabytes << L" MiB\r\n";
    text << L"Java: " << (settings_.javaExecutable.empty() ? std::wstring(L"automático") : anonymizePath(settings_.javaExecutable.wstring())) << L"\r\n";
    text << L"Pasta do jogo: " << anonymizePath(gameDirectory().wstring()) << L"\r\n";
    text << L"Modo: " << (originalMode_ ? L"conta original" : L"nickname") << L"\r\n";
    text << L"Mods: " << (mods_.manifestKnown ? std::to_wstring(mods_.server.size()) : std::wstring(L"?")) << L" do servidor, "
         << mods_.user.size() << L" do jogador\r\n";
    text << L"Estado: " << viewState_.headline << L" — " << viewState_.operationMessage << L"\r\n";
    // The end of the installation log usually explains the failure.
    std::ifstream log(gameDirectory() / L"logs" / L"installation.log", std::ios::binary);
    if (log) {
        std::deque<std::string> lines;
        for (std::string line; std::getline(log, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(std::move(line));
            if (lines.size() > 30) lines.pop_front();
        }
        text << L"\r\nFim do installation.log:\r\n";
        for (const auto& line : lines) text << anonymizePath(utf8ToWide(line)) << L"\r\n";
    }
    const auto value = text.str();
    bool copied = false;
    if (OpenClipboard(window_)) {
        EmptyClipboard();
        const auto bytes = (value.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (auto* target = static_cast<wchar_t*>(GlobalLock(memory))) {
                std::memcpy(target, value.c_str(), bytes);
                GlobalUnlock(memory);
                copied = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!copied) GlobalFree(memory);
        }
        CloseClipboard();
    }
    setHelpMessage(copied ? L"Informações copiadas. Cole no grupo ou no relato do GitHub." :
        L"Não foi possível usar a área de transferência. Tente de novo.", !copied);
}

// ---------------------------------------------------------------------------------------------
// News read state
// ---------------------------------------------------------------------------------------------

void MainPage::loadNewsReadState()
{
    const auto directory = LauncherSettingsStore::dataDirectory();
    newsRead_.load(directory);
    const auto file = directory / L"last-version.txt";
    const std::string current(LauncherConfig::NumericVersion);
    std::string previous;
    std::error_code ignored;
    const bool existed = std::filesystem::exists(file, ignored);
    if (existed) {
        std::ifstream input(file, std::ios::binary);
        std::getline(input, previous);
        while (!previous.empty() && (previous.back() == '\r' || previous.back() == ' ')) previous.pop_back();
    }
    if (previous != current) {
        // Existing players (a saved nickname) are told what changed; a first install is not.
        const bool returning = existed || std::filesystem::exists(NicknameStore::filePath(directory), ignored);
        if (!returning) { newsRead_.markSeen(NewsFeed::releaseNotes()); newsRead_.save(); }
        std::filesystem::create_directories(directory, ignored);
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output << current << "\n";
    }
    applyNewsReadState();
}

void MainPage::applyNewsReadState()
{
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t today[16]{};
    swprintf_s(today, L"%04u-%02u-%02u", now.wYear, now.wMonth, now.wDay);
    newsRead_.apply(viewState_.news, today);
}

int MainPage::spotlightIndex() const
{
    // The newest unread item the player has not set aside; server news come before the
    // launcher's own notes in the list.
    for (std::size_t i = 0; i < viewState_.news.size(); ++i)
        if (viewState_.news[i].unread && !newsRead_.dismissed(viewState_.news[i])) return static_cast<int>(i);
    return -1;
}

int MainPage::unreadNewsCount() const
{
    return static_cast<int>(std::count_if(viewState_.news.begin(), viewState_.news.end(), [](const NewsItem& item) { return item.unread; }));
}

void MainPage::dismissSpotlight()
{
    const int index = spotlightIndex();
    if (index < 0) return;
    // "Later" only quiets the spotlight; the item stays unread with its badge and tag.
    newsRead_.markDismissed(viewState_.news[static_cast<std::size_t>(index)]);
    newsRead_.save();
    if (const auto* read = findControl(SpotlightReadButtonId); read && GetFocus() == read->window)
        if (const auto* play = findControl(PlayButtonId); play && play->window) SetFocus(play->window);
    redraw();
}

void MainPage::markNewsRead()
{
    // Opening the list reads everything on it. The flags stay while the page is open so the
    // NOVO tags can still be seen there.
    for (const auto& item : viewState_.news) newsRead_.markSeen(item);
    newsRead_.save();
}

// ---------------------------------------------------------------------------------------------
// Modpack changelog
// ---------------------------------------------------------------------------------------------

void MainPage::updateModpackChanges()
{
    if (!mods_.manifestKnown) return;
    modpackFiles_.clear();
    for (const auto& mod : mods_.server) modpackFiles_.push_back(mod.name);
    const auto directory = LauncherSettingsStore::dataDirectory();
    const auto previous = ModpackChangelog::load(directory);
    if (!previous) {
        // First time with this feature (or a first install): this list is the baseline.
        ModpackChangelog::save(directory, modpackFiles_);
        modpackChangesVisible_ = false;
        return;
    }
    modpackChanges_ = ModpackChangelog::compare(*previous, modpackFiles_);
    modpackChangesVisible_ = !modpackChanges_.empty();
}

void MainPage::acknowledgeModpackChanges()
{
    if (!modpackFiles_.empty()) ModpackChangelog::save(LauncherSettingsStore::dataDirectory(), modpackFiles_);
    modpackChangesVisible_ = false;
    modpackChanges_ = {};
    redraw();
}

// ---------------------------------------------------------------------------------------------
// Play-time levels
// ---------------------------------------------------------------------------------------------

std::optional<std::uint64_t> MainPage::serverSecondsForAccount() const
{
    const auto account = rankingName();
    if (account.empty() || !serverTotalSeconds_ || _wcsicmp(serverTotalName_.c_str(), account.c_str()) != 0) return std::nullopt;
    return serverTotalSeconds_;
}

void MainPage::checkMilestone()
{
    // Only the server's number counts, the same one the ranking shows.
    const auto seconds = serverSecondsForAccount();
    if (!seconds) return;
    const auto account = rankingName();
    const int level = RankTier::level(*seconds);
    const int celebrated = milestones_.celebrated(account);
    if (celebrated < 0 && level <= 1) {
        // A new player: nothing to celebrate yet, start counting from here.
        milestones_.setCelebrated(account, level);
        milestones_.save();
        return;
    }
    // A known player who climbed, or the first time this player is seen with a level
    // already earned: celebrate the highest one reached (never each one in between).
    if (level <= celebrated || (celebration_ == level && celebrationName_ == account)) {
        if (celebration_ == level) celebrationSeconds_ = *seconds;
        return;
    }
    celebration_ = level;
    celebrationSeconds_ = *seconds;
    celebrationName_ = account;
    if (hiddenToTray_ && trayIcon_.visible())
        trayIcon_.notify(std::wstring(L"Nova conquista: nível ") + RankTier::name(level) + L"!",
            std::to_wstring(*seconds / 3600) + L" horas jogadas. Abra o launcher para ver.");
    redraw();
}

void MainPage::dismissCelebration()
{
    if (celebration_ == 0) return;
    milestones_.setCelebrated(celebrationName_, celebration_);
    milestones_.save();
    celebration_ = 0;
    if (const auto* button = findControl(CelebrationButtonId); button && GetFocus() == button->window)
        if (const auto* play = findControl(PlayButtonId); play && play->window) SetFocus(play->window);
    redraw();
}

// ---------------------------------------------------------------------------------------------
// Screenshots
// ---------------------------------------------------------------------------------------------

ScreenshotsModel MainPage::screenshotsModel(bool preview) const
{
    ScreenshotsModel model;
    model.group = galleryTab_;
    model.firstRow = screenshotRow_;
    model.viewer = screenshotViewer_;
    model.message = screenshotsMessage_;
    model.messageError = screenshotsMessageError_;
    model.entrance = preview ? 1.0F : pageFade_;
    model.time = preview ? 0.0F : time_;
    const auto now = wallClock();
    if (galleryTab_) {
        model.thumbnails = &galleryThumbnails_;
        model.loading = galleryLoading_;
        model.unavailable = galleryUnavailable_;
        model.cards.reserve(galleryPhotos_.size());
        for (const auto& photo : galleryPhotos_)
            model.cards.push_back({photo.id, photo.name, ScreenshotLibrary::formatWhen(photo.created, now),
                galleryShared_.byId(photo.id) ? L"Sua" : L"", false});
        if (const auto* photo = currentGalleryPhoto()) {
            model.viewerTitle = L"Captura de " + photo->name;
            model.viewerDetail = std::to_wstring(screenshotViewer_ + 1) + L" de " + std::to_wstring(galleryPhotos_.size()) + L" · " +
                ScreenshotLibrary::formatDate(photo->created);
            if (photo->width > 0) model.viewerDetail += L" · " + std::to_wstring(photo->width) + L" × " + std::to_wstring(photo->height);
            model.viewerMine = galleryShared_.byId(photo->id) != nullptr;
            model.viewerWidth = photo->width;
            model.viewerHeight = photo->height;
            if (galleryImageId_ == photo->id) model.viewerImage = galleryImage_.get();
        }
        return model;
    }
    model.thumbnails = &screenshotThumbnails_;
    model.sharing = galleryBusy_;
    model.cards.reserve(screenshots_.size());
    for (const auto& entry : screenshots_)
        model.cards.push_back({entry.path.wstring(), ScreenshotLibrary::formatDate(entry.modified), ModInventory::formatSize(entry.size),
            entry.vanilla ? L"1.8.9" : L"", galleryShared_.bySource(GallerySharedStore::sourceKey(entry.path, entry.size)) != nullptr});
    if (const auto* shot = currentScreenshot()) {
        model.viewerTitle = ScreenshotLibrary::formatDate(shot->modified);
        model.viewerDetail = std::to_wstring(screenshotViewer_ + 1) + L" de " + std::to_wstring(screenshots_.size());
        if (viewerImagePath_ == shot->path.wstring()) {
            model.viewerImage = viewerImage_.get();
            model.viewerWidth = viewerWidth_;
            model.viewerHeight = viewerHeight_;
            if (viewerWidth_ > 0) model.viewerDetail += L" · " + std::to_wstring(viewerWidth_) + L" × " + std::to_wstring(viewerHeight_);
        }
        model.viewerDetail += L" · " + ModInventory::formatSize(shot->size) + (shot->vanilla ? L" · Minecraft 1.8.9" : L" · Modpack Eternal");
        model.viewerShared = galleryShared_.bySource(GallerySharedStore::sourceKey(shot->path, shot->size)) != nullptr;
    }
    return model;
}

const ScreenshotEntry* MainPage::currentScreenshot() const
{
    if (galleryTab_ || screenshotViewer_ < 0 || static_cast<std::size_t>(screenshotViewer_) >= screenshots_.size()) return nullptr;
    return &screenshots_[static_cast<std::size_t>(screenshotViewer_)];
}

const GalleryPhoto* MainPage::currentGalleryPhoto() const
{
    if (!galleryTab_ || screenshotViewer_ < 0 || static_cast<std::size_t>(screenshotViewer_) >= galleryPhotos_.size()) return nullptr;
    return &galleryPhotos_[static_cast<std::size_t>(screenshotViewer_)];
}

std::size_t MainPage::shotCount() const
{
    return galleryTab_ ? galleryPhotos_.size() : screenshots_.size();
}

void MainPage::refreshScreenshots()
{
    screenshots_ = ScreenshotLibrary::scan(gameDirectory() / L"screenshots", VanillaMinecraftService::gameDirectory() / L"screenshots");
    screenshotRow_ = std::min(screenshotRow_, ScreenshotsView::lastFirstRow(screenshots_.size()));
    if (screenshotViewer_ >= static_cast<int>(screenshots_.size())) screenshotViewer_ = static_cast<int>(screenshots_.size()) - 1;
    // Forget thumbnails of files that are gone (or were replaced under the same name).
    std::map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>> kept;
    for (const auto& entry : screenshots_)
        if (const auto found = screenshotThumbnails_.find(entry.path.wstring()); found != screenshotThumbnails_.end())
            kept.insert(*found);
    screenshotThumbnails_ = std::move(kept);
    requestScreenshotImages();
}

void MainPage::requestScreenshotImages()
{
    if (!window_ || page_ != LauncherPage::Screenshots) return;
    if (galleryTab_) { requestGalleryImages(); return; }
    // One worker at a time; when it finishes, this runs again for whatever is still missing.
    if (screenshotLoading_) return;
    if (screenshotThumbnails_.size() > 72) {
        // Keep memory flat on large folders: only thumbnails around the visible rows stay.
        const std::size_t visibleFirst = screenshotRow_ * Layout::ScreenshotColumns;
        const std::size_t keepFirst = visibleFirst > 24 ? visibleFirst - 24 : 0;
        const std::size_t keepLast = std::min(screenshots_.size(), visibleFirst + ScreenshotsView::Visible + 24);
        std::map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>> kept;
        for (std::size_t i = keepFirst; i < keepLast; ++i)
            if (const auto found = screenshotThumbnails_.find(screenshots_[i].path.wstring()); found != screenshotThumbnails_.end())
                kept.insert(*found);
        screenshotThumbnails_ = std::move(kept);
    }
    struct Job { std::filesystem::path path; bool large; };
    std::vector<Job> jobs;
    const auto* shot = currentScreenshot();
    if (shot && viewerImagePath_ != shot->path.wstring()) jobs.push_back({shot->path, true});
    // Visible cards first, then the next row so scrolling down finds them ready.
    const std::size_t first = screenshotRow_ * Layout::ScreenshotColumns;
    const std::size_t last = std::min(screenshots_.size(), first + ScreenshotsView::Visible + Layout::ScreenshotColumns);
    for (std::size_t i = first; i < last; ++i)
        if (!screenshotThumbnails_.count(screenshots_[i].path.wstring())) jobs.push_back({screenshots_[i].path, false});
    if (shot) {
        // In the viewer, the neighbours' thumbnails stand in while their full pictures load.
        for (const int neighbour : {screenshotViewer_ - 1, screenshotViewer_ + 1})
            if (neighbour >= 0 && static_cast<std::size_t>(neighbour) < screenshots_.size() &&
                !screenshotThumbnails_.count(screenshots_[static_cast<std::size_t>(neighbour)].path.wstring()))
                jobs.push_back({screenshots_[static_cast<std::size_t>(neighbour)].path, false});
    }
    if (jobs.empty()) return;
    // Thumbnails decode at 1.5x the card so they stay sharp on high-DPI screens.
    constexpr int thumbWidth = 416, thumbHeight = 234;
    if (screenshotThread_.joinable()) screenshotThread_.join();
    screenshotLoading_ = true;
    const HWND window = window_;
    screenshotThread_ = std::jthread([window, jobs = std::move(jobs)](std::stop_token stop) {
        const auto post = [window](ScreenshotImage* image) {
            if (!PostMessageW(window, ScreenshotImageMessage, 0, reinterpret_cast<LPARAM>(image))) delete image;
        };
        for (const auto& job : jobs) {
            if (stop.stop_requested()) break;
            auto image = std::make_unique<ScreenshotImage>(job.large ? ScreenshotLibrary::load(job.path, 1920, 1080, false)
                                                                     : ScreenshotLibrary::load(job.path, thumbWidth, thumbHeight, true));
            post(image.release());
        }
        post(nullptr);
    });
    // Keep the placeholders shimmering while the worker runs.
    redraw();
}

void MainPage::openScreenshot(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= shotCount()) return;
    const bool wasGrid = screenshotViewer_ < 0;
    screenshotViewer_ = index;
    screenshotsMessage_.clear();
    screenshotsMessageError_ = false;
    if (galleryTab_) {
        if (galleryImageId_ != galleryPhotos_[static_cast<std::size_t>(index)].id) { galleryImage_.reset(); galleryImageId_.clear(); }
    } else if (viewerImagePath_ != screenshots_[static_cast<std::size_t>(index)].path.wstring()) {
        viewerImage_.reset();
        viewerImagePath_.clear();
        viewerWidth_ = viewerHeight_ = 0;
    }
    // Keep the grid scrolled to the picture, so going back lands where it is.
    const auto row = static_cast<std::size_t>(index) / Layout::ScreenshotColumns;
    if (row < screenshotRow_) screenshotRow_ = row;
    else if (row >= screenshotRow_ + Layout::ScreenshotRows) screenshotRow_ = row - Layout::ScreenshotRows + 1;
    requestScreenshotImages();
    redraw();
    if (wasGrid)
        if (const auto* back = findControl(ViewerBackButtonId); back && back->window && GetFocus() != back->window) {
            const HWND focus = GetFocus();
            // Focus left a grid card that is now hidden; put it on the viewer.
            if (!focus || !(GetWindowLongPtrW(focus, GWL_STYLE) & WS_VISIBLE)) SetFocus(back->window);
        }
}

void MainPage::closeScreenshot()
{
    if (screenshotViewer_ < 0) return;
    const auto index = static_cast<std::size_t>(screenshotViewer_);
    screenshotViewer_ = -1;
    screenshotsMessage_.clear();
    screenshotsMessageError_ = false;
    viewerImage_.reset();
    viewerImagePath_.clear();
    galleryImage_.reset();
    galleryImageId_.clear();
    requestScreenshotImages();
    redraw();
    // Focus returns to the card of the picture that was open.
    const auto slot = index - std::min(index, screenshotRow_ * Layout::ScreenshotColumns);
    if (slot < ScreenshotsView::Visible)
        if (const auto* card = findControl(ScreenshotCardFirstId + static_cast<int>(slot)); card && card->window) SetFocus(card->window);
}

void MainPage::deleteScreenshot()
{
    const auto* shot = currentScreenshot();
    if (!shot) return;
    const auto name = shot->path.filename().wstring();
    const auto prompt = L"Mover \"" + name + L"\" para a Lixeira?\n\nVocê pode recuperá-la de lá se mudar de ideia.";
    if (MessageBoxW(window_, prompt.c_str(), L"Excluir captura", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) != IDOK) return;
    // Release the decoded picture before the file moves.
    viewerImage_.reset();
    viewerImagePath_.clear();
    std::wstring error;
    const auto path = shot->path;
    if (!ScreenshotLibrary::remove(path, error)) { setScreenshotsMessage(error, true); return; }
    screenshotThumbnails_.erase(path.wstring());
    const int index = screenshotViewer_;
    refreshScreenshots();
    if (screenshots_.empty()) { screenshotViewer_ = -1; setScreenshotsMessage(name + L" foi para a Lixeira."); return; }
    screenshotViewer_ = std::min(index, static_cast<int>(screenshots_.size()) - 1);
    openScreenshot(screenshotViewer_);
    setScreenshotsMessage(name + L" foi para a Lixeira.");
}

void MainPage::setScreenshotsMessage(std::wstring message, bool error)
{
    screenshotsMessage_ = std::move(message);
    screenshotsMessageError_ = error;
    redraw();
}

void MainPage::copyCurrentPicture()
{
    std::wstring error;
    bool copied = false;
    if (const auto* shot = currentScreenshot()) copied = ScreenshotLibrary::copyToClipboard(window_, shot->path, error);
    else if (const auto* photo = currentGalleryPhoto()) {
        if (galleryImage_ && galleryImageId_ == photo->id) copied = ScreenshotLibrary::copyToClipboard(window_, *galleryImage_, error);
        else error = L"Espere a captura terminar de carregar.";
    } else return;
    if (copied) setScreenshotsMessage(L"Copiada! Cole no Discord ou no WhatsApp com Ctrl+V.");
    else setScreenshotsMessage(error, true);
}

// ---------------------------------------------------------------------------------------------
// Group gallery
// ---------------------------------------------------------------------------------------------

void MainPage::showGalleryTab(bool group)
{
    if (galleryTab_ == group) return;
    closeScreenshot();
    galleryTab_ = group;
    screenshotRow_ = 0;
    screenshotsMessage_.clear();
    screenshotsMessageError_ = false;
    // The viewer's copy and remove buttons sit elsewhere in each tab.
    layoutControls();
    if (group) {
        // Opening the tab always brings the newest list.
        galleryLoaded_ = false;
        galleryUnavailable_ = false;
    }
    requestScreenshotImages();
    redraw();
}

void MainPage::requestGalleryImages()
{
    if (!window_ || page_ != LauncherPage::Screenshots || !galleryTab_ || galleryWorking_) return;
    struct Job { std::wstring id; bool full; };
    std::vector<Job> jobs;
    const bool needList = !galleryLoaded_ && !galleryUnavailable_;
    if (!needList) {
        if (const auto* photo = currentGalleryPhoto(); photo && galleryImageId_ != photo->id) jobs.push_back({photo->id, true});
        const std::size_t first = screenshotRow_ * Layout::ScreenshotColumns;
        const std::size_t last = std::min(galleryPhotos_.size(), first + ScreenshotsView::Visible + Layout::ScreenshotColumns);
        for (std::size_t i = first; i < last; ++i)
            if (!galleryThumbnails_.count(galleryPhotos_[i].id)) jobs.push_back({galleryPhotos_[i].id, false});
        if (jobs.empty()) return;
    }
    if (galleryThread_.joinable()) galleryThread_.join();
    galleryWorking_ = true;
    galleryLoading_ = needList;
    galleryThread_ = std::jthread([window = window_, url = galleryBaseUrl_, needList, jobs = std::move(jobs)](std::stop_token stop) {
        const auto post = [window](GalleryEvent* event) {
            if (!PostMessageW(window, GalleryEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
        };
        if (needList) {
            auto* event = new GalleryEvent;
            event->kind = GalleryEvent::Kind::List;
            event->list = GalleryApi::fetchList(url, stop);
            post(event);
        }
        for (const auto& job : jobs) {
            if (stop.stop_requested()) break;
            auto* event = new GalleryEvent;
            event->kind = GalleryEvent::Kind::Image;
            event->id = job.id;
            event->full = job.full;
            event->bitmap = GalleryApi::fetchImage(url, job.id, job.full, stop);
            post(event);
        }
        auto* done = new GalleryEvent;
        done->kind = GalleryEvent::Kind::Done;
        post(done);
    });
    redraw();
}

void MainPage::handleGalleryEvent(GalleryEvent& event)
{
    using Kind = GalleryEvent::Kind;
    switch (event.kind) {
    case Kind::List:
        galleryLoading_ = false;
        if (event.list.success) {
            const std::wstring open = currentGalleryPhoto() ? currentGalleryPhoto()->id : std::wstring{};
            galleryPhotos_ = std::move(event.list.photos);
            galleryLoaded_ = true;
            galleryUnavailable_ = false;
            // Keep the open picture open if it is still there.
            screenshotViewer_ = -1;
            for (std::size_t i = 0; !open.empty() && i < galleryPhotos_.size(); ++i)
                if (galleryPhotos_[i].id == open) screenshotViewer_ = static_cast<int>(i);
            screenshotRow_ = std::min(screenshotRow_, ScreenshotsView::lastFirstRow(galleryPhotos_.size()));
            // Forget thumbnails of pictures that left the gallery.
            std::map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>> kept;
            for (const auto& photo : galleryPhotos_)
                if (const auto found = galleryThumbnails_.find(photo.id); found != galleryThumbnails_.end()) kept.insert(*found);
            galleryThumbnails_ = std::move(kept);
        } else {
            galleryUnavailable_ = true;
        }
        break;
    case Kind::Image:
        if (event.full) {
            if (const auto* photo = currentGalleryPhoto(); photo && photo->id == event.id) {
                galleryImage_.reset(event.bitmap.release());
                galleryImageId_ = event.id;
                if (!galleryImage_) setScreenshotsMessage(L"Não foi possível carregar esta captura.", true);
            }
        } else {
            // A failed download still stores an entry so it is not retried in a loop.
            galleryThumbnails_[event.id] = std::shared_ptr<Gdiplus::Bitmap>(event.bitmap.release());
        }
        break;
    case Kind::Done:
        if (galleryThread_.joinable()) galleryThread_.join();
        galleryWorking_ = false;
        requestGalleryImages();
        break;
    case Kind::Upload:
        if (galleryActionThread_.joinable()) galleryActionThread_.join();
        galleryBusy_ = false;
        if (event.upload.success) {
            galleryShared_.add({event.upload.id, event.upload.deleteToken, event.source});
            galleryShared_.save();
            galleryLoaded_ = false;  // the next visit to the group tab shows it
            setScreenshotsMessage(event.upload.pending ? L"Enviada! Ela aparece na galeria quando um admin aprovar."
                                                       : L"Compartilhada! Já está na Galeria do grupo.");
        } else {
            setScreenshotsMessage(event.upload.errorMessage, true);
        }
        break;
    case Kind::Remove:
        if (galleryActionThread_.joinable()) galleryActionThread_.join();
        galleryBusy_ = false;
        if (event.success) {
            galleryShared_.erase(event.id);
            galleryShared_.save();
            galleryThumbnails_.erase(event.id);
            const auto found = std::find_if(galleryPhotos_.begin(), galleryPhotos_.end(), [&](const GalleryPhoto& p) { return p.id == event.id; });
            if (found != galleryPhotos_.end()) galleryPhotos_.erase(found);
            if (galleryPhotos_.empty()) closeScreenshot();
            else if (screenshotViewer_ >= 0) openScreenshot(std::min(screenshotViewer_, static_cast<int>(galleryPhotos_.size()) - 1));
            setScreenshotsMessage(L"Captura removida da galeria do grupo.");
        } else {
            setScreenshotsMessage(event.error, true);
        }
        break;
    case Kind::Report:
        if (galleryActionThread_.joinable()) galleryActionThread_.join();
        galleryBusy_ = false;
        setScreenshotsMessage(event.success ? L"Denúncia enviada. Obrigado por cuidar da galeria!" : event.error, !event.success);
        break;
    }
    if (page_ == LauncherPage::Screenshots) redraw();
}

void MainPage::shareScreenshot()
{
    const auto* shot = currentScreenshot();
    if (!shot || galleryBusy_) return;
    const auto nickname = rankingName();
    if (!NicknameStore::isValid(nickname)) {
        setScreenshotsMessage(L"Escolha um nickname válido na tela inicial antes de compartilhar.", true);
        return;
    }
    const auto prompt = L"Compartilhar esta captura com o grupo?\n\nEla aparece na Galeria do grupo para todo mundo, com o seu nickname (" +
        nickname + L"). Você pode remover depois por este computador.";
    if (MessageBoxW(window_, prompt.c_str(), L"Compartilhar captura", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    if (galleryActionThread_.joinable()) galleryActionThread_.join();
    galleryBusy_ = true;
    setScreenshotsMessage(L"Enviando para a galeria do grupo...");
    galleryActionThread_ = std::jthread([window = window_, url = galleryBaseUrl_, path = shot->path, nickname,
                                            source = GallerySharedStore::sourceKey(shot->path, shot->size)](std::stop_token stop) {
        auto* event = new GalleryEvent;
        event->kind = GalleryEvent::Kind::Upload;
        event->source = source;
        event->upload = GalleryApi::upload(url, path, nickname, stop);
        if (!PostMessageW(window, GalleryEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
}

void MainPage::removeGalleryPhoto()
{
    const auto* photo = currentGalleryPhoto();
    const auto* shared = photo ? galleryShared_.byId(photo->id) : nullptr;
    if (!shared || galleryBusy_) return;
    if (MessageBoxW(window_, L"Remover esta captura da Galeria do grupo?\n\nO arquivo no seu computador continua onde está.",
            L"Remover da galeria", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) != IDOK) return;
    if (galleryActionThread_.joinable()) galleryActionThread_.join();
    galleryBusy_ = true;
    galleryActionThread_ = std::jthread([window = window_, url = galleryBaseUrl_, id = photo->id, token = shared->token](std::stop_token) {
        auto* event = new GalleryEvent;
        event->kind = GalleryEvent::Kind::Remove;
        event->id = id;
        event->success = GalleryApi::remove(url, id, token, event->error);
        if (!PostMessageW(window, GalleryEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
    redraw();
}

void MainPage::reportGalleryPhoto()
{
    const auto* photo = currentGalleryPhoto();
    if (!photo || galleryBusy_) return;
    const auto prompt = L"Denunciar a captura de " + photo->name + L"?\n\nUse para conteúdo ofensivo ou que não é do jogo. "
        L"Com denúncias de 3 pessoas ela sai da galeria até um admin olhar.";
    if (MessageBoxW(window_, prompt.c_str(), L"Denunciar captura", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) return;
    if (galleryActionThread_.joinable()) galleryActionThread_.join();
    galleryBusy_ = true;
    galleryActionThread_ = std::jthread([window = window_, url = galleryBaseUrl_, id = photo->id](std::stop_token) {
        auto* event = new GalleryEvent;
        event->kind = GalleryEvent::Kind::Report;
        event->id = id;
        event->success = GalleryApi::report(url, id, event->error);
        if (!PostMessageW(window, GalleryEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
    redraw();
}

// ---------------------------------------------------------------------------------------------
// Play flow
// ---------------------------------------------------------------------------------------------

std::wstring MainPage::nicknameText() const
{
    if (!nicknameInput_) return {};
    std::array<wchar_t, 32> nickname{};
    GetWindowTextW(nicknameInput_, nickname.data(), static_cast<int>(nickname.size()));
    return nickname.data();
}

bool MainPage::gameRunning() const
{
    return playSession_.active() || (minecraftLauncherService_ && minecraftLauncherService_->isGameRunning());
}

bool MainPage::canRepair() const
{
    return viewState_.playEnabled && !updatingLauncher_ && !installation_.running && !gameRunning();
}

void MainPage::handlePrimaryAction()
{
    if (!viewState_.playEnabled) return;
    playingVanilla_ = vanillaSelected_;
    std::wstring settingsError;
    if (!LauncherSettingsStore::validate(settings_, settingsError)) {
        showPage(LauncherPage::Settings);
        settingsPage_.setMessage(settingsError, true);
        return;
    }
    if (originalMode_) {
        if (!microsoftIdentity_.online()) { startMicrosoftSignIn(); return; }
        pendingIdentity_ = microsoftIdentity_;
        pendingNickname_ = microsoftIdentity_.name;
        startGame();
        return;
    }
    // Conta Eternal: sign in first, then check the session and the agent before playing.
    if (!eternalSignedIn()) { openLogin(true); return; }
    prepareEternalPlay();
}

// ---------------------------------------------------------------------------------------------
// Conta Eternal
// ---------------------------------------------------------------------------------------------

float MainPage::loginTarget() const
{
    return loginOpen_ && page_ == LauncherPage::Home && !installation_.visible ? 1.0F : 0.0F;
}

void MainPage::openLogin(bool playAfter, std::wstring notice)
{
    playAfterLogin_ = playAfter;
    loginNotice_ = std::move(notice);
    loginError_.clear();
    if (page_ != LauncherPage::Home) showPage(LauncherPage::Home);
    levelsOpen_ = false;
    loginOpen_ = true;
    // Players coming from nickname mode keep their name as the username.
    if (loginUser_ && GetWindowTextLengthW(loginUser_) == 0) {
        const auto saved = NicknameStore::load(LauncherSettingsStore::dataDirectory());
        if (!saved.empty()) SetWindowTextW(loginUser_, saved.c_str());
    }
    redraw();
    const HWND target = loginUser_ && GetWindowTextLengthW(loginUser_) == 0 ? loginUser_ : loginPassword_;
    if (target) SetFocus(target);
    // The fields sit on mode-dependent rows.
    layoutControls();
}

void MainPage::closeLogin()
{
    if (!loginOpen_) return;
    loginOpen_ = false;
    loginBusy_ = false;
    loginError_.clear();
    loginNotice_.clear();
    for (const HWND field : {loginPassword_, loginConfirm_}) if (field) SetWindowTextW(field, L"");
    if (loginMode_ != 0) { loginMode_ = 0; layoutControls(); }
    redraw();
    if (const auto* play = findControl(PlayButtonId); play && play->window) SetFocus(play->window);
}

HWND MainPage::loginField(int field) const
{
    return field == 0 ? loginUser_ : field == 1 ? loginPassword_ : loginConfirm_;
}

void MainPage::setLoginMode(int mode)
{
    loginMode_ = mode;
    loginError_.clear();
    loginNotice_.clear();
    for (const HWND field : {loginPassword_, loginConfirm_}) if (field) SetWindowTextW(field, L"");
    if (loginPassword_)
        SendMessageW(loginPassword_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(mode == 0 ? L"Sua senha" : L"Crie uma senha (mínimo 8)"));
    // The fields move between rows, so the native controls follow.
    layoutControls();
    redraw();
    const HWND target = mode == 1 ? loginPassword_ : loginUser_ && GetWindowTextLengthW(loginUser_) == 0 ? loginUser_ : loginPassword_;
    if (target) SetFocus(target);
}

void MainPage::submitLogin()
{
    if (loginBusy_ || !loginUser_ || !loginPassword_ || !loginConfirm_) return;
    std::array<wchar_t, 64> user{};
    GetWindowTextW(loginUser_, user.data(), static_cast<int>(user.size()));
    // Passwords live only in these buffers and the request, and are wiped right after.
    std::array<wchar_t, 160> password{}, confirmation{};
    GetWindowTextW(loginPassword_, password.data(), static_cast<int>(password.size()));
    GetWindowTextW(loginConfirm_, confirmation.data(), static_cast<int>(confirmation.size()));
    const auto wipe = [&] {
        SecureZeroMemory(password.data(), password.size() * sizeof(wchar_t));
        SecureZeroMemory(confirmation.data(), confirmation.size() * sizeof(wchar_t));
    };
    std::wstring username = loginMode_ == 1 ? loginFirstName_ : std::wstring(user.data());
    while (!username.empty() && username.back() == L' ') username.pop_back();
    while (!username.empty() && username.front() == L' ') username.erase(username.begin());
    const auto fail = [&](std::wstring message, HWND focus) { wipe(); loginError_ = std::move(message); SetFocus(focus); redraw(); };
    if (username.empty()) { fail(L"Digite seu nick.", loginUser_); return; }
    const std::size_t length = wcslen(password.data());

    if (loginMode_ == 0 && length == 0) {
        // Only the nickname: maybe a reserved account waiting for its first password.
        wipe();
        loginBusy_ = true;
        loginError_.clear();
        if (eternalThread_.joinable()) eternalThread_.join();
        eternalThread_ = std::jthread([window = window_, url = std::wstring(LauncherConfig::FileServerBaseUrl), username](std::stop_token stop) {
            auto* result = new EternalAccount::FirstAccess(EternalAccount::checkFirstAccess(url, username, stop));
            if (result->name.empty()) result->name = username;
            if (!PostMessageW(window, EternalFirstAccessMessage, 0, reinterpret_cast<LPARAM>(result))) delete result;
        });
        redraw();
        return;
    }
    if (loginMode_ != 0) {
        if (!NicknameStore::isValid(username)) { fail(L"Use de 3 a 16 letras, números ou _ no nick.", loginUser_); return; }
        if (length < 8) { fail(L"A senha precisa ter pelo menos 8 caracteres.", loginPassword_); return; }
        if (wcscmp(password.data(), confirmation.data()) != 0) {
            SetWindowTextW(loginConfirm_, L"");
            fail(L"As duas senhas não são iguais.", loginConfirm_);
            return;
        }
    }
    loginBusy_ = true;
    loginError_.clear();
    if (eternalThread_.joinable()) eternalThread_.join();
    std::wstring secret(password.data());
    wipe();
    const auto clientToken = eternalSession_.clientToken.empty() ? EternalAccount::newClientToken() : eternalSession_.clientToken;
    // First access sets the password, a new account is created, then all three sign in.
    eternalThread_ = std::jthread([window = window_, url = accountsBaseUrl_, files = std::wstring(LauncherConfig::FileServerBaseUrl),
                                      mode = loginMode_, username, secret = std::move(secret), clientToken](std::stop_token stop) mutable {
        auto* result = new EternalAuthResult;
        std::wstring error;
        const bool ready = mode == 1 ? EternalAccount::claimFirstAccess(files, username, secret, error, stop)
                         : mode == 2 ? EternalAccount::registerAccount(url, username, secret, error, stop) : true;
        if (ready) *result = EternalAccount::authenticate(url, username, secret, clientToken, stop);
        else result->errorMessage = error;
        SecureZeroMemory(secret.data(), secret.size() * sizeof(wchar_t));
        if (!PostMessageW(window, EternalAuthMessage, 0, reinterpret_cast<LPARAM>(result))) delete result;
    });
    redraw();
}

void MainPage::prepareEternalPlay()
{
    if (eternalPreparing_ || !eternalSignedIn()) return;
    eternalPreparing_ = true;
    viewState_.operation = LauncherOperation::SigningIn;
    viewState_.headline = L"Conferindo sua conta";
    viewState_.operationMessage = L"Confirmando a Conta Eternal de " + eternalSession_.name + L"...";
    viewState_.primaryActionLabel = L"CONECTANDO";
    viewState_.playEnabled = false;
    redraw();
    if (eternalThread_.joinable()) eternalThread_.join();
    eternalThread_ = std::jthread([window = window_, url = accountsBaseUrl_, session = eternalSession_,
                                      data = LauncherSettingsStore::dataDirectory()](std::stop_token stop) {
        auto* ready = new EternalReady;
        ready->auth = EternalAccount::ensureValid(url, session, stop);
        if (ready->auth.success) ready->agent = EternalAccount::ensureAuthlibInjector(data, ready->error, stop);
        if (!PostMessageW(window, EternalReadyMessage, 0, reinterpret_cast<LPARAM>(ready))) delete ready;
    });
}

void MainPage::signOutEternal()
{
    if (!eternalSignedIn() || gameRunning() || installation_.running) return;
    const auto prompt = L"Sair da Conta Eternal de " + eternalSession_.name + L"?\n\nPara jogar de novo será preciso digitar a senha.";
    if (MessageBoxW(window_, prompt.c_str(), L"Sair da conta", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) != IDOK) return;
    // Revoking the token on the server is best effort; the local session goes now.
    std::thread([url = accountsBaseUrl_, session = eternalSession_] { EternalAccount::signOut(url, session); }).detach();
    EternalSessionStore::clear(LauncherSettingsStore::dataDirectory());
    eternalSession_ = {};
    serverTotalSeconds_.reset();
    redraw();
}

// ---------------------------------------------------------------------------------------------
// Skins
// ---------------------------------------------------------------------------------------------

std::filesystem::path MainPage::skinLibraryDirectory() const
{
    return LauncherSettingsStore::dataDirectory() / L"skins";
}

SkinsModel MainPage::skinsModel(bool preview) const
{
    SkinsModel model;
    model.signedIn = preview || eternalSignedIn();
    model.account = preview ? previewEternalName_ : eternalSession_.name;
    model.currentLoading = currentSkinLoading_;
    model.busy = skinBusy_;
    model.selected = skinSelected_;
    model.firstRow = skinRow_;
    model.slimChoice = skinSlimChoice_;
    model.selectedRemovable = skinSelected_ > 0 && static_cast<std::size_t>(skinSelected_) <= skinLibrary_.size();
    model.message = skinsMessage_;
    model.messageError = skinsMessageError_;
    model.entrance = preview ? 1.0F : pageFade_;
    model.time = preview ? 0.0F : time_;
    model.sharing = skinSharing_;
    model.explore = skinExplore_;
    if (skinExplore_) {
        model.selected = exploreSelected_;
        model.firstRow = exploreRow_;
        model.selectedRemovable = false;
        model.exploreLoading = exploreLoading_;
        model.exploreUnavailable = exploreUnavailable_;
        for (int i = 0; i < static_cast<int>(exploreCount()); ++i) {
            const auto* skin = exploreSkin(i);
            model.cards.push_back({skin->title, &skin->texture, skin->slim, false, skin->group, skin->searched});
        }
        if (const auto* skin = exploreSkin(exploreSelected_)) {
            const bool community = !skin->group && !skin->searched;
            const bool mine = community && skinsShared_.byId(skin->id);
            model.selectedDetail = skin->searched ? skin->author : skin->group ? std::wstring(L"Em uso no servidor")
                                 : mine ? std::wstring(L"Compartilhada por você") : L"Compartilhada por " + skin->author;
            model.selectedMine = mine;
            model.selectedCommunity = community;
        }
        model.searching = skinSearching_;
        model.searchPreview = preview ? previewSkinSearch_ : std::wstring{};
        return model;
    }
    model.cards.push_back({currentSkinKnown_ && !currentSkin_ ? L"Padrão" : L"Sua skin atual",
        currentSkin_ ? &*currentSkin_ : nullptr, currentSkinSlim_, true});
    for (const auto& entry : skinLibrary_) model.cards.push_back({entry.name, &entry.texture, entry.slim, false});
    if (model.selectedRemovable) {
        model.selectedShared = skinsShared_.bySource(librarySourceKey(static_cast<std::size_t>(skinSelected_ - 1))) != nullptr;
        model.selectedShareable = !model.selectedShared;
    }
    return model;
}

// ---------------------------------------------------------------------------------------------
// Skins: Explorar

std::wstring MainPage::librarySourceKey(std::size_t libraryIndex) const
{
    if (libraryIndex >= skinLibrary_.size()) return {};
    const auto& entry = skinLibrary_[libraryIndex];
    return GallerySharedStore::sourceKey(entry.path, entry.png.size());
}

const MainPage::ExploreSkin* MainPage::exploreSkin(int index) const
{
    if (index < 0) return nullptr;
    if (exploreSearch_ && index == 0) return &*exploreSearch_;
    const auto i = static_cast<std::size_t>(index) - (exploreSearch_ ? 1 : 0);
    if (i < exploreGroup_.size()) return &exploreGroup_[i];
    if (i - exploreGroup_.size() < exploreCommunity_.size()) return &exploreCommunity_[i - exploreGroup_.size()];
    return nullptr;
}

void MainPage::showSkinsTab(bool explore)
{
    if (skinExplore_ == explore) return;
    skinExplore_ = explore;
    skinsMessage_.clear();
    skinsMessageError_ = false;
    if (explore) {
        if (const auto* skin = exploreSkin(exploreSelected_)) skinSlimChoice_ = skin->slim;
        loadExplore();
    } else {
        skinSlimChoice_ = skinSelected_ == 0 ? currentSkinSlim_ : skinLibrary_[static_cast<std::size_t>(skinSelected_ - 1)].slim;
    }
    layoutControls();
    redraw();
}

void MainPage::selectExplore(int index)
{
    const auto* skin = exploreSkin(index);
    if (!skin) return;
    exploreSelected_ = index;
    skinSlimChoice_ = skin->slim;
    skinsMessage_.clear();
    skinsMessageError_ = false;
    layoutControls();
    redraw();
}

void MainPage::loadExplore()
{
    if (exploreLoading_ || exploreLoaded_ || !playtimeSyncEnabled_) return;
    exploreLoading_ = true;
    exploreUnavailable_ = false;
    std::vector<std::wstring> names;
    for (const auto& entry : leaderboard_.players) names.push_back(entry.name);
    if (eternalSignedIn() && std::find(names.begin(), names.end(), eternalSession_.name) == names.end()) names.push_back(eternalSession_.name);
    const auto generation = ++exploreGeneration_;
    if (exploreThread_.joinable()) exploreThread_.join();
    exploreThread_ = std::jthread([window = window_, accounts = accountsBaseUrl_, gallery = galleryBaseUrl_, names, generation](std::stop_token stop) {
        // Shared skins first: one list and small files. The players' skins follow.
        auto* community = new SkinEvent;
        community->kind = SkinEvent::Kind::ExploreCommunity;
        community->generation = generation;
        auto list = GalleryApi::fetchSkins(gallery, stop);
        community->success = list.success;
        for (auto& shared : list.skins) {
            auto texture = SkinRenderer::decode(shared.png);
            if (!texture) continue;
            community->explore.push_back({shared.id, shared.title, shared.name, false, shared.slim, std::move(shared.png), std::move(*texture)});
        }
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(community))) delete community;
        if (stop.stop_requested()) return;

        auto* group = new SkinEvent;
        group->kind = SkinEvent::Kind::ExploreGroup;
        group->generation = generation;
        group->last = true;
        for (const auto& profile : EternalAccount::lookupProfiles(accounts, names, stop)) {
            if (stop.stop_requested()) break;
            auto skin = EternalAccount::fetchSkin(accounts, profile.uuid, stop);
            if (!skin || !skin->custom || skin->png.empty()) continue;  // the default skin is not worth exploring
            auto texture = SkinRenderer::decode(skin->png);
            if (!texture) continue;
            group->explore.push_back({{}, profile.name, profile.name, true, skin->slim, std::move(skin->png), std::move(*texture)});
        }
        group->success = true;
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(group))) delete group;
    });
    redraw();
}

void MainPage::searchSkin()
{
    if (skinSearching_ || !skinSearch_) return;
    std::array<wchar_t, 32> text{};
    GetWindowTextW(skinSearch_, text.data(), static_cast<int>(text.size()));
    std::wstring name(text.data());
    while (!name.empty() && name.back() == L' ') name.pop_back();
    while (!name.empty() && name.front() == L' ') name.erase(name.begin());
    if (name.empty()) { SetFocus(skinSearch_); return; }
    if (!NicknameStore::isValid(name)) { setSkinsMessage(L"Nicks têm de 3 a 16 letras, números ou _.", true); return; }
    skinSearching_ = true;
    setSkinsMessage(L"Buscando a skin de " + name + L"...");
    if (skinSearchThread_.joinable()) skinSearchThread_.join();
    skinSearchThread_ = std::jthread([window = window_, accounts = accountsBaseUrl_, name](std::stop_token stop) {
        auto* event = new SkinEvent;
        event->kind = SkinEvent::Kind::Search;
        event->id = name;
        // Original accounts first (the point of searching), then the Conta Eternal of that name.
        auto original = SkinLookup::fetchOriginal(name, stop);
        bool unavailable = original.outcome == SkinLookupOutcome::Unavailable;
        bool defaultSkin = original.outcome == SkinLookupOutcome::Found && !original.custom;
        if (original.custom) {
            if (auto texture = SkinRenderer::decode(original.png))
                event->explore.push_back({{}, original.name, L"Conta original do Minecraft", false, original.slim, std::move(original.png),
                                          std::move(*texture), true});
        } else if (!stop.stop_requested()) {
            for (const auto& profile : EternalAccount::lookupProfiles(accounts, {name}, stop)) {
                const auto skin = EternalAccount::fetchSkin(accounts, profile.uuid, stop);
                if (!skin) { unavailable = true; continue; }
                if (!skin->custom || skin->png.empty()) { defaultSkin = true; continue; }
                if (auto texture = SkinRenderer::decode(skin->png))
                    event->explore.push_back({{}, profile.name, L"Conta Eternal", false, skin->slim, skin->png, std::move(*texture), true});
            }
        }
        if (event->explore.empty())
            event->error = defaultSkin ? name + L" usa a skin padrão do Minecraft." :
                           unavailable ? L"Não foi possível buscar agora. Tente de novo em instantes." :
                                         L"Nenhuma conta com o nick " + name + L" foi encontrada.";
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
    layoutControls();
    redraw();
}

void MainPage::handleExploreEvent(SkinEvent& event)
{
    if (event.kind == SkinEvent::Kind::Search) {
        if (skinSearchThread_.joinable()) skinSearchThread_.join();
        skinSearching_ = false;
        if (event.explore.empty()) {
            setSkinsMessage(event.error, true);
            layoutControls();
            return;
        }
        exploreSearch_ = std::move(event.explore.front());
        exploreSelected_ = 0;
        exploreRow_ = 0;
        skinSlimChoice_ = exploreSearch_->slim;
        setSkinsMessage(L"Skin de " + exploreSearch_->title + L" encontrada. Use ou salve na sua biblioteca.");
        layoutControls();
        return;
    }
    const auto selectedKey = [this] {
        const auto* skin = exploreSkin(exploreSelected_);
        return skin ? (skin->searched ? L"s:" + skin->title : skin->group ? L"g:" + skin->title : L"c:" + skin->id) : std::wstring{};
    }();
    switch (event.kind) {
    case SkinEvent::Kind::ExploreCommunity:
    case SkinEvent::Kind::ExploreGroup:
        if (event.generation != exploreGeneration_) return;
        if (event.kind == SkinEvent::Kind::ExploreCommunity) {
            exploreCommunity_ = std::move(event.explore);
            exploreUnavailable_ = !event.success;
        } else {
            exploreGroup_ = std::move(event.explore);
        }
        if (event.last) {
            if (exploreThread_.joinable()) exploreThread_.join();
            exploreLoading_ = false;
            exploreLoaded_ = true;
        }
        break;
    case SkinEvent::Kind::Share:
        if (exploreActionThread_.joinable()) exploreActionThread_.join();
        skinSharing_ = false;
        if (!event.shared.success) { setSkinsMessage(event.shared.errorMessage, true); break; }
        skinsShared_.add({event.shared.id, event.shared.deleteToken, event.source});
        skinsShared_.save();
        if (!event.explore.empty() && !event.shared.pending) {
            event.explore.front().id = event.shared.id;
            exploreCommunity_.insert(exploreCommunity_.begin(), std::move(event.explore.front()));
        }
        setSkinsMessage(event.shared.pending ? L"Enviada! Ela aparece em Explorar depois que um admin aprovar."
                                             : L"Compartilhada! Agora ela aparece em Explorar para todo mundo do grupo.");
        break;
    case SkinEvent::Kind::CommunityRemove:
    case SkinEvent::Kind::CommunityReport:
        if (exploreActionThread_.joinable()) exploreActionThread_.join();
        skinSharing_ = false;
        if (!event.success) { setSkinsMessage(event.error, true); break; }
        if (event.kind == SkinEvent::Kind::CommunityRemove) {
            skinsShared_.erase(event.id);
            skinsShared_.save();
        }
        exploreCommunity_.erase(std::remove_if(exploreCommunity_.begin(), exploreCommunity_.end(),
            [&event](const ExploreSkin& skin) { return skin.id == event.id; }), exploreCommunity_.end());
        setSkinsMessage(event.kind == SkinEvent::Kind::CommunityRemove ? L"Pronto: a skin saiu de Explorar."
                                                                      : L"Denúncia enviada. A skin some daqui para você; obrigado!");
        break;
    default:
        return;
    }
    // The selection stays on the same skin when the list around it changes.
    exploreSelected_ = 0;
    bool kept = false;
    for (int i = 0; i < static_cast<int>(exploreCount()); ++i) {
        const auto* skin = exploreSkin(i);
        if (!selectedKey.empty() && (skin->searched ? L"s:" + skin->title : skin->group ? L"g:" + skin->title : L"c:" + skin->id) == selectedKey) { exploreSelected_ = i; kept = true; }
    }
    exploreRow_ = std::min(exploreRow_, SkinsView::lastFirstRow(exploreCount()));
    // A newly selected skin brings its own arm model; a kept one keeps the player's choice.
    if (skinExplore_ && !kept)
        if (const auto* skin = exploreSkin(exploreSelected_)) skinSlimChoice_ = skin->slim;
    layoutControls();
    redraw();
}

void MainPage::saveExploreSkin()
{
    const auto* skin = exploreSkin(exploreSelected_);
    if (!skin) return;
    for (const auto& entry : skinLibrary_)
        if (entry.png == skin->png) { setSkinsMessage(L"Esta skin já está na sua biblioteca, como " + entry.name + L"."); return; }
    std::wstring name = skin->title;
    for (auto& c : name) if (c < 32 || std::wstring_view(L"<>:\"/\\|?*").find(c) != std::wstring_view::npos) c = L'_';
    if (name.empty()) name = L"Skin";
    const auto directory = skinLibraryDirectory();
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    auto target = directory / (name + L".png");
    for (int n = 2; std::filesystem::exists(target, ignored); ++n) target = directory / (name + L" (" + std::to_wstring(n) + L").png");
    {
        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        output.write(skin->png.data(), static_cast<std::streamsize>(skin->png.size()));
        if (!output) { setSkinsMessage(L"Não foi possível salvar a skin na biblioteca.", true); return; }
    }
    const auto selected = exploreSelected_;
    refreshSkinLibrary();
    exploreSelected_ = selected;
    skinSlimChoice_ = skin->slim;
    setSkinsMessage(target.stem().wstring() + L" entrou na sua biblioteca.");
}

void MainPage::shareLibrarySkin()
{
    if (skinSharing_ || skinSelected_ <= 0 || static_cast<std::size_t>(skinSelected_) > skinLibrary_.size()) return;
    if (!eternalSignedIn()) { setSkinsMessage(L"Entre na sua Conta Eternal para compartilhar skins.", true); return; }
    const auto& entry = skinLibrary_[static_cast<std::size_t>(skinSelected_ - 1)];
    const auto title = GalleryApi::skinTitle(entry.name);
    const auto prompt = L"Compartilhar \"" + title + L"\" com o grupo?\n\nEla aparece em Explorar para todo mundo, com o nome " +
        eternalSession_.name + L". Você pode tirar de lá quando quiser.";
    if (MessageBoxW(window_, prompt.c_str(), L"Compartilhar skin", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    skinSharing_ = true;
    setSkinsMessage(L"Compartilhando a skin...");
    ExploreSkin shared{{}, title, eternalSession_.name, false, skinSlimChoice_, entry.png, entry.texture};
    if (exploreActionThread_.joinable()) exploreActionThread_.join();
    exploreActionThread_ = std::jthread([window = window_, url = galleryBaseUrl_, name = eternalSession_.name, title, source = librarySourceKey(
        static_cast<std::size_t>(skinSelected_ - 1)), shared = std::move(shared)](std::stop_token stop) mutable {
        auto* event = new SkinEvent;
        event->kind = SkinEvent::Kind::Share;
        event->shared = GalleryApi::shareSkin(url, shared.png, name, title, shared.slim, stop);
        event->source = source;
        event->explore.push_back(std::move(shared));
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
    layoutControls();
}

void MainPage::removeCommunitySkin()
{
    const auto* skin = exploreSkin(exploreSelected_);
    if (skinSharing_ || !skin || skin->group || skin->searched) return;
    const auto* mine = skinsShared_.byId(skin->id);
    if (!mine) return;
    const auto prompt = L"Tirar \"" + skin->title + L"\" de Explorar?\n\nEla continua na sua biblioteca.";
    if (MessageBoxW(window_, prompt.c_str(), L"Remover skin compartilhada", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) != IDOK) return;
    skinSharing_ = true;
    if (exploreActionThread_.joinable()) exploreActionThread_.join();
    exploreActionThread_ = std::jthread([window = window_, url = galleryBaseUrl_, id = skin->id, token = mine->token](std::stop_token) {
        auto* event = new SkinEvent;
        event->kind = SkinEvent::Kind::CommunityRemove;
        event->id = id;
        event->success = GalleryApi::removeSkin(url, id, token, event->error);
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
    redraw();
}

void MainPage::reportCommunitySkin()
{
    const auto* skin = exploreSkin(exploreSelected_);
    if (skinSharing_ || !skin || skin->group || skin->searched) return;
    const auto prompt = L"Denunciar \"" + skin->title + L"\", de " + skin->author +
        L"?\n\nUse para skins ofensivas. Com algumas denúncias ela some de Explorar até um admin olhar.";
    if (MessageBoxW(window_, prompt.c_str(), L"Denunciar skin", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) return;
    skinSharing_ = true;
    if (exploreActionThread_.joinable()) exploreActionThread_.join();
    exploreActionThread_ = std::jthread([window = window_, url = galleryBaseUrl_, id = skin->id](std::stop_token) {
        auto* event = new SkinEvent;
        event->kind = SkinEvent::Kind::CommunityReport;
        event->id = id;
        event->success = GalleryApi::reportSkin(url, id, event->error);
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
    redraw();
}

void MainPage::refreshSkinLibrary()
{
    const auto selectedPath = skinSelected_ > 0 && static_cast<std::size_t>(skinSelected_) <= skinLibrary_.size()
        ? skinLibrary_[static_cast<std::size_t>(skinSelected_ - 1)].path : std::filesystem::path{};
    skinLibrary_.clear();
    std::error_code error;
    const auto directory = skinLibraryDirectory();
    if (std::filesystem::is_directory(directory, error)) {
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
            std::error_code ignored;
            if (!it->is_regular_file(ignored) || it->path().extension() != L".png" || it->file_size(ignored) > 512 * 1024) continue;
            std::ifstream input(it->path(), std::ios::binary);
            std::string png((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            auto texture = SkinRenderer::decode(png);
            if (!texture) continue;
            const bool slim = SkinRenderer::looksSlim(*texture);
            skinLibrary_.push_back({it->path().stem().wstring(), it->path(), std::move(png), std::move(*texture), slim});
        }
    }
    std::sort(skinLibrary_.begin(), skinLibrary_.end(), [](const SkinEntry& a, const SkinEntry& b) {
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    skinSelected_ = 0;
    for (std::size_t i = 0; !selectedPath.empty() && i < skinLibrary_.size(); ++i)
        if (skinLibrary_[i].path == selectedPath) skinSelected_ = static_cast<int>(i + 1);
    skinRow_ = std::min(skinRow_, SkinsView::lastFirstRow(1 + skinLibrary_.size()));
    skinSlimChoice_ = skinSelected_ == 0 ? currentSkinSlim_ : skinLibrary_[static_cast<std::size_t>(skinSelected_ - 1)].slim;
}

void MainPage::fetchCurrentSkin()
{
    if (!eternalSignedIn() || currentSkinLoading_) return;
    currentSkinLoading_ = true;
    if (skinFetchThread_.joinable()) skinFetchThread_.join();
    skinFetchThread_ = std::jthread([window = window_, url = accountsBaseUrl_, uuid = eternalSession_.uuid](std::stop_token stop) {
        auto* event = new SkinEvent;
        event->kind = SkinEvent::Kind::Current;
        event->skin = EternalAccount::fetchSkin(url, uuid, stop);
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
}

void MainPage::selectSkin(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) > skinLibrary_.size()) return;
    skinSelected_ = index;
    skinSlimChoice_ = index == 0 ? currentSkinSlim_ : skinLibrary_[static_cast<std::size_t>(index - 1)].slim;
    skinsMessage_.clear();
    skinsMessageError_ = false;
    redraw();
}

void MainPage::addSkin()
{
    std::array<wchar_t, 32768> file{};
    OPENFILENAMEW dialog{sizeof(OPENFILENAMEW)};
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"Skins do Minecraft (*.png)\0*.png\0\0";
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrTitle = L"Adicionar skin";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return;
    const std::filesystem::path source(file.data());
    std::ifstream input(source, std::ios::binary);
    std::string png((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::wstring error;
    if (!EternalAccount::validSkinPng(png, error)) { setSkinsMessage(error, true); return; }
    if (!SkinRenderer::decode(png)) { setSkinsMessage(L"Não foi possível ler esta skin.", true); return; }
    const auto directory = skinLibraryDirectory();
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    // Keep the file's name; a number keeps two skins with the same name apart.
    auto target = directory / source.filename();
    for (int n = 2; std::filesystem::exists(target, ignored); ++n)
        target = directory / (source.stem().wstring() + L" (" + std::to_wstring(n) + L").png");
    std::filesystem::copy_file(source, target, ignored);
    if (ignored) { setSkinsMessage(L"Não foi possível copiar a skin para a biblioteca.", true); return; }
    refreshSkinLibrary();
    for (std::size_t i = 0; i < skinLibrary_.size(); ++i)
        if (skinLibrary_[i].path == target) {
            selectSkin(static_cast<int>(i + 1));
            skinRow_ = std::min((i + 1) / Layout::SkinColumns, SkinsView::lastFirstRow(1 + skinLibrary_.size()));
        }
    setSkinsMessage(target.stem().wstring() + L" entrou na sua biblioteca.");
}

void MainPage::removeSkin()
{
    if (skinSelected_ <= 0 || static_cast<std::size_t>(skinSelected_) > skinLibrary_.size()) return;
    const auto entry = skinLibrary_[static_cast<std::size_t>(skinSelected_ - 1)];
    const auto prompt = L"Mover \"" + entry.name + L"\" para a Lixeira?\n\nIsso só tira a skin da biblioteca; a skin em uso não muda.";
    if (MessageBoxW(window_, prompt.c_str(), L"Remover skin", MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) != IDOK) return;
    std::wstring error;
    if (!ScreenshotLibrary::remove(entry.path, error)) { setSkinsMessage(L"Não foi possível remover a skin.", true); return; }
    skinSelected_ = 0;
    refreshSkinLibrary();
    setSkinsMessage(entry.name + L" foi para a Lixeira.");
}

void MainPage::useSelectedSkin()
{
    if (skinBusy_ || !eternalSignedIn()) return;
    std::string png;
    if (skinExplore_) {
        if (const auto* skin = exploreSkin(exploreSelected_)) png = skin->png;
    }
    else if (skinSelected_ == 0) png = currentSkinPng_;  // same picture, new arm model
    else if (static_cast<std::size_t>(skinSelected_) <= skinLibrary_.size()) png = skinLibrary_[static_cast<std::size_t>(skinSelected_ - 1)].png;
    if (png.empty()) return;
    skinBusy_ = true;
    setSkinsMessage(L"Enviando a skin...");
    if (skinActionThread_.joinable()) skinActionThread_.join();
    skinActionThread_ = std::jthread([window = window_, url = accountsBaseUrl_, session = eternalSession_, png, slim = skinSlimChoice_]
                                     (std::stop_token) {
        auto* event = new SkinEvent;
        event->kind = SkinEvent::Kind::Upload;
        event->success = EternalAccount::uploadSkin(url, session, png, slim, event->error);
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
}

void MainPage::resetSkin()
{
    if (skinBusy_ || !eternalSignedIn()) return;
    if (MessageBoxW(window_, L"Voltar para a skin padrão?\n\nSuas skins da biblioteca continuam aqui.", L"Skin padrão",
            MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) != IDOK) return;
    skinBusy_ = true;
    if (skinActionThread_.joinable()) skinActionThread_.join();
    skinActionThread_ = std::jthread([window = window_, url = accountsBaseUrl_, session = eternalSession_](std::stop_token) {
        auto* event = new SkinEvent;
        event->kind = SkinEvent::Kind::Reset;
        event->success = EternalAccount::resetSkin(url, session, event->error);
        if (!PostMessageW(window, SkinEventMessage, 0, reinterpret_cast<LPARAM>(event))) delete event;
    });
    redraw();
}

void MainPage::setSkinsMessage(std::wstring message, bool error)
{
    skinsMessage_ = std::move(message);
    skinsMessageError_ = error;
    redraw();
}

std::wstring MainPage::avatarCacheName() const
{
    // Eternal skins and Mojang skins of the same name are different pictures.
    return originalMode_ ? avatarName() : L"eternal:" + avatarName();
}

void MainPage::startEternalSkinLookup()
{
    if (!eternalSignedIn()) return;
    const auto key = skinKey(avatarCacheName());
    if (skinPending_ == key) return;
    skinPending_ = key;
    skinThread_ = std::jthread([window = window_, url = accountsBaseUrl_, uuid = eternalSession_.uuid, name = avatarCacheName()]
                               (std::stop_token stop) {
        auto* result = new SkinLookupResult{name, SkinLookupOutcome::Unavailable, {}};
        if (const auto skin = EternalAccount::fetchSkin(url, uuid, stop)) {
            result->outcome = SkinLookupOutcome::NotFound;
            if (skin->custom)
                if (const auto face = SkinLookup::decodeFace(skin->png)) { result->outcome = SkinLookupOutcome::Found; result->face = *face; }
        }
        if (stop.stop_requested()) { delete result; return; }
        if (!PostMessageW(window, SkinLookupResultMessage, 0, reinterpret_cast<LPARAM>(result))) delete result;
    });
}

bool MainPage::offlineReady() const
{
    return serverOffline_ && minecraftLauncherService_ && minecraftLauncherService_->validateInstallation().success;
}

void MainPage::startGame()
{
    // The 1.8.9 only needs Mojang, so the state of the group's file server does not matter.
    if (playingVanilla_) { beginPlay(); return; }
    if (offlineReady()) {
        launchPreparedMinecraft(InstallationResult{InstallationOutcome::Success}, true);
        redraw();
        return;
    }
    if (serverOffline_) {
        // Nothing installed yet: the server is required, so try it again and play if it answers.
        refreshBeforePlay_ = true;
        retryServer();
        return;
    }
    beginPlay();
}

void MainPage::retryServer()
{
    if (fileServerPending_ || installation_.running || gameRunning() || updatingLauncher_) return;
    viewState_.operation = LauncherOperation::CheckingUpdates;
    viewState_.headline = L"Conectando ao servidor";
    viewState_.operationMessage = L"Procurando o servidor de arquivos do grupo...";
    viewState_.primaryActionLabel = L"CONECTANDO";
    viewState_.playEnabled = false;
    redraw();
    checkFileServerAsync();
}

void MainPage::showOfflineState(std::wstring message)
{
    serverOffline_ = true;
    viewState_.operation = LauncherOperation::OfflineReady;
    viewState_.headline = L"Servidor de arquivos fora do ar";
    viewState_.operationMessage = std::move(message);
    viewState_.primaryActionLabel = L"JOGAR ASSIM MESMO";
    viewState_.playEnabled = true;
    if (viewState_.statusCards.size() >= 3)
        viewState_.statusCards[2] = {L"SERVIDOR", L"Fora do ar", L"Jogando com os arquivos locais", StatusTone::Warning};
    if (window_) SetTimer(window_, ServerRetryTimerId, 60 * 1000, nullptr);
}

void MainPage::startRepair()
{
    if (!canRepair()) return;
    playingVanilla_ = false;
    std::wstring settingsError;
    if (!LauncherSettingsStore::validate(settings_, settingsError)) {
        showPage(LauncherPage::Settings);
        settingsPage_.setMessage(settingsError, true);
        return;
    }
    beginPlay(true);
}

void MainPage::startMicrosoftSignIn()
{
    if (microsoftAuthThread_.joinable()) microsoftAuthThread_.join();
    viewState_.operation = LauncherOperation::SigningIn;
    viewState_.headline = L"Entrando com Microsoft";
    viewState_.operationMessage = L"Aguarde o código de confirmação...";
    viewState_.primaryActionLabel = L"CONECTANDO";
    viewState_.playEnabled = false;
    redraw();
    microsoftAuthThread_ = std::jthread([this](std::stop_token stopToken) {
        MicrosoftAuthService service;
        auto result = service.signIn(stopToken, [this, stopToken](const MicrosoftDeviceCode& code) {
            if (stopToken.stop_requested()) return;
            auto* transferred = new MicrosoftDeviceCode(code);
            if (!PostMessage(window_, MicrosoftDeviceCodeMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
        });
        if (stopToken.stop_requested()) return;
        auto* transferred = new MicrosoftAuthResult(std::move(result));
        if (!PostMessage(window_, MicrosoftAuthResultMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
    });
}

void MainPage::beginPlay(bool repairOnly)
{
    if (installation_.running || updatingLauncher_ || gameRunning()) return;
    if (installationThread_.joinable()) installationThread_.join();
    fileServerThread_.request_stop();
    if (fileServerThread_.joinable()) fileServerThread_.join();
    repairOnly_ = repairOnly;
    // Playing means the player has seen what changed in the modpack.
    if (modpackChangesVisible_ && !playingVanilla_) acknowledgeModpackChanges();
    installation_.begin();
    installationMailbox_.begin(installation_.attempt);
    viewState_.operation = LauncherOperation::PreparingMinecraft;
    viewState_.headline = repairOnly ? L"Reparando a instalação" : L"Preparando seu mundo";
    viewState_.operationMessage = playingVanilla_ ? L"Conferindo Java e arquivos da 1.8.9..." : L"Conferindo Java e arquivos do grupo...";
    viewState_.primaryActionLabel = repairOnly ? L"REPARANDO" : L"PREPARANDO";
    viewState_.playEnabled = false;
    viewState_.progressVisible = true;
    viewState_.progressPercent = 0;
    showPage(LauncherPage::Home);
    SetTimer(window_, InstallationProgressTimerId, 200, nullptr);
    redraw();
    const auto attempt = installation_.attempt;
    installationThread_ = std::jthread([this, attempt, vanilla = playingVanilla_](std::stop_token stop) {
        const InstallationProgressCallback publish = [this, attempt](const InstallationProgress& value) {
            if (installationMailbox_.publish(attempt, value, InstallationTransferMeter::Clock::now()))
                PostMessageW(window_, InstallationProgressMessage, 0, 0);
        };
        InstallationCoordinator coordinator(*fileServerService_, *javaRuntimeService_, *gameInstallationService_,
            *minecraftLauncherService_, settings_);
        auto result = vanilla ? VanillaMinecraftService{}.prepare(stop, publish) : coordinator.run(stop, publish);
        installationMailbox_.finish(attempt, std::move(result));
        PostMessageW(window_, InstallationProgressMessage, 0, 0);
    });
}

void MainPage::pollGameProcess()
{
    if (minecraftLauncherService_->isGameRunning()) return;
    KillTimer(window_, GameProcessTimerId);
    const auto session = playSession_.active() ? playSession_.elapsedSeconds(PlaySession::Clock::now()) : 0;
    const bool wasHidden = hiddenToTray_;
    finishPlaySession();
    playingVanilla_ = false;
    viewState_.operation = LauncherOperation::ReadyToPlay;
    viewState_.headline = L"Minecraft encerrado";
    viewState_.operationMessage = session > 0 ? L"Sessão de " + PlaytimeStore::formatDuration(session) +
        L". Você pode jogar de novo quando quiser." : std::wstring(L"Você pode iniciar uma nova sessão quando quiser.");
    viewState_.primaryActionLabel = L"JOGAR";
    viewState_.playEnabled = true;
    if (serverOffline_) {
        // Still without the file server: the next session also uses the local files.
        viewState_.operation = LauncherOperation::OfflineReady;
        viewState_.primaryActionLabel = L"JOGAR ASSIM MESMO";
    }
    refreshMods();
    if (exitAfterGame_) {
        // The window was closed during the session: finish closing now that the time is saved.
        PostMessageW(window_, WM_CLOSE, 0, 0);
        return;
    }
    if (wasHidden) restoreFromTray();
    redraw();
}

void MainPage::saveSettings()
{
    if (!viewState_.playEnabled || gameRunning()) return;
    LauncherSettings candidate;
    if (!settingsPage_.collect(candidate)) return;
    std::wstring error;
    if (!LauncherSettingsStore::save(LauncherSettingsStore::dataDirectory(), candidate, error)) {
        settingsPage_.setMessage(error, true);
        return;
    }
    settings_ = std::move(candidate);
    installation_.visible = false;
    settingsWarning_.clear();
    // No active operation or game can hold these services while the form is editable.
    minecraftLauncherService_ = std::make_unique<MinecraftLauncherService>(settings_);
    gameInstallationService_ = std::make_unique<GameInstallationService>(settings_);
    settingsPage_.open(settings_, {});
    settingsPage_.setMessage(L"Preferências salvas. Valem a partir do próximo Jogar.");
    viewState_.operation = LauncherOperation::CheckingUpdates;
    viewState_.headline = L"Conferindo a instância";
    viewState_.operationMessage = L"Verificando a instalação e o servidor de arquivos...";
    viewState_.primaryActionLabel = L"VERIFICANDO";
    viewState_.playEnabled = false;
    refreshMods();
    redraw();
    checkFileServerAsync();
}

void MainPage::checkFileServerAsync()
{
    // The HTTP call never runs on the window thread, keeping the
    // nickname field, drawing and other controls responsive.
    if (fileServerThread_.joinable()) fileServerThread_.join();
    fileServerPending_ = true;
    fileServerThread_ = std::jthread([this](std::stop_token stopToken) {
        auto response = fileServerService_->fetchManifest(stopToken);
        if (stopToken.stop_requested()) return;
        auto* transferred = new FileServerManifestResponse(std::move(response));
        // If the window was closed during the request, nobody will receive the message.
        if (!PostMessage(window_, FileServerResultMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
    });
}

void MainPage::fetchNewsAsync()
{
    if (newsRequested_) return;
    newsRequested_ = true;
    newsLoading_ = true;
    newsThread_ = std::jthread([this](std::stop_token stop) {
        auto result = NewsFeed::fetch(LauncherConfig::FileServerBaseUrl, stop);
        if (stop.stop_requested()) return;
        auto* transferred = new NewsFeedResult(std::move(result));
        if (!PostMessage(window_, NewsResultMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
    });
}

std::wstring MainPage::rankingName() const
{
    return originalMode_ ? microsoftIdentity_.name : eternalSession_.name;
}

void MainPage::fetchLeaderboardAsync()
{
    if (!playtimeSyncEnabled_ || leaderboardPending_) return;
    leaderboardPending_ = true;
    SetTimer(window_, LeaderboardTimerId, 5 * 60 * 1000, nullptr);
    leaderboardThread_ = std::jthread([window = window_, name = rankingName()](std::stop_token stop) {
        auto result = PlaytimeApi::fetchLeaderboard(LauncherConfig::FileServerBaseUrl, name,
            static_cast<int>(PlaytimeApi::LeaderboardLimit), stop);
        if (stop.stop_requested()) return;
        auto* transferred = new LeaderboardResult(std::move(result));
        if (!PostMessageW(window, LeaderboardResultMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
    });
}

std::wstring MainPage::avatarName() const
{
    return originalMode_ ? microsoftIdentity_.name : eternalSession_.name;
}

const SkinLookupResult* MainPage::cachedSkin(const std::wstring& name) const
{
    const auto found = skins_.find(skinKey(name));
    return found == skins_.end() ? nullptr : &found->second;
}

void MainPage::scheduleSkinLookup()
{
    // Wait until typing pauses, so each keystroke does not query Mojang.
    if (window_) SetTimer(window_, SkinLookupTimerId, 600, nullptr);
}

void MainPage::startSkinLookup()
{
    if (!originalMode_) {
        if (!playtimeSyncEnabled_) return;
        const auto* cached = cachedSkin(avatarCacheName());
        if (!cached || cached->outcome == SkinLookupOutcome::Unavailable) startEternalSkinLookup();
        return;
    }
    const auto name = avatarName();
    if (!playtimeSyncEnabled_ || !NicknameStore::isValid(name)) return;
    const auto* cached = cachedSkin(name);
    if (cached && cached->outcome != SkinLookupOutcome::Unavailable) return;
    const auto key = skinKey(name);
    if (skinPending_ == key) return;
    skinPending_ = key;
    // Replacing the thread cancels a lookup for a name that is no longer shown.
    skinThread_ = std::jthread([window = window_, name](std::stop_token stop) {
        auto result = SkinLookup::lookup(name, stop);
        if (stop.stop_requested()) return;
        auto* transferred = new SkinLookupResult(std::move(result));
        if (!PostMessageW(window, SkinLookupResultMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
    });
}

void MainPage::startPlaytimeSync(const PlaytimeRecord& local)
{
    if (!playtimeSyncEnabled_ || !NicknameStore::isValid(pendingNickname_)) return;
    auto update = [window = window_, name = pendingNickname_](std::optional<std::uint64_t> total, bool finished) {
        auto* transferred = new PlaytimeSyncUpdate{name, total, finished};
        if (!PostMessageW(window, PlaytimeSyncMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
    };
    playtimeSyncThread_ = std::jthread([name = pendingNickname_, local, directory = LauncherSettingsStore::dataDirectory(),
                                           update = std::move(update)](std::stop_token stop) {
        PlaytimeApi::runSession(LauncherConfig::FileServerBaseUrl, name, local, directory, update, stop);
    });
}

void MainPage::checkLauncherUpdateAsync(bool confirmed)
{
    updatingLauncher_ = true;
    applyLauncherUpdateProgress({LauncherUpdatePhase::Checking, 0});
    const auto executable = LauncherUpdateRuntime::executablePath();
    launcherUpdateThread_ = std::jthread([this, executable, confirmed](std::stop_token stop) {
        LauncherUpdateTransport transport;
        LauncherUpdateService service;
        auto result = service.prepare(executable, stop, [this, stop](const LauncherUpdateProgress& value) {
            if (stop.stop_requested()) return;
            auto* progress = new LauncherUpdateProgress(value);
            if (!PostMessageW(window_, LauncherUpdateProgressMessage, 0, reinterpret_cast<LPARAM>(progress))) delete progress;
        }, transport, confirmed);
        if (stop.stop_requested()) return;
        auto* transferred = new LauncherUpdateResult(std::move(result));
        if (!PostMessageW(window_, LauncherUpdateResultMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
    });
}

void MainPage::applyLauncherUpdateProgress(const LauncherUpdateProgress& progress)
{
    viewState_.playEnabled = false;
    viewState_.progressVisible = progress.phase == LauncherUpdatePhase::Downloading;
    viewState_.progressPercent = progress.percent;
    switch (progress.phase) {
    case LauncherUpdatePhase::Checking:
        viewState_.operation = LauncherOperation::CheckingLauncherUpdate;
        viewState_.headline = L"Verificando atualização";
        viewState_.operationMessage = L"Consultando a versão mais recente do launcher...";
        viewState_.primaryActionLabel = L"VERIFICANDO";
        break;
    case LauncherUpdatePhase::Downloading:
        viewState_.operation = LauncherOperation::DownloadingLauncherUpdate;
        viewState_.headline = L"Baixando atualização";
        viewState_.operationMessage = L"O launcher vai reabrir ao concluir. Seus dados serão preservados.";
        viewState_.primaryActionLabel = L"BAIXANDO";
        break;
    case LauncherUpdatePhase::Applying:
        viewState_.operation = LauncherOperation::ApplyingLauncherUpdate;
        viewState_.headline = L"Aplicando atualização";
        viewState_.operationMessage = L"Verificando o arquivo e preparando a troca...";
        viewState_.primaryActionLabel = L"ATUALIZANDO";
        break;
    case LauncherUpdatePhase::Restarting:
        viewState_.operation = LauncherOperation::RestartingLauncher;
        viewState_.headline = L"Reabrindo launcher";
        viewState_.operationMessage = L"Tudo pronto. A nova versão vai abrir automaticamente.";
        viewState_.primaryActionLabel = L"REABRINDO";
        break;
    }
    redraw();
}

void MainPage::applyFileServerResult(const FileServerManifestResponse& response)
{
    fetchNewsAsync();
    fetchLeaderboardAsync();
    if (gameRunning() || installation_.running) {
        // A background retry answered while playing or preparing: keep the current state.
        if (response.success) { fileServerManifest_ = response.content; serverOffline_ = false; KillTimer(window_, ServerRetryTimerId); }
        return;
    }
    if (installation_.visible) return;
    if (viewState_.statusCards.size() < 3) return;
    const auto installation = minecraftLauncherService_->validateInstallation();
    updateInstallationState(installation);

    auto& serverCard = viewState_.statusCards[2];
    if (response.success) {
        fileServerManifest_ = response.content;
        serverOffline_ = false;
        if (window_) KillTimer(window_, ServerRetryTimerId);
        refreshMods();
        updateModpackChanges();
        serverCard.value = L"Online";
        serverCard.detail = L"Respondeu em " + std::to_wstring(response.elapsedMilliseconds) + L" ms";
        serverCard.tone = StatusTone::Success;

        if (installation.issue == MinecraftLaunchIssue::InvalidConfiguration) {
            refreshBeforePlay_ = false;
            viewState_.operation = LauncherOperation::LaunchError;
            viewState_.headline = L"Confira as configurações";
            viewState_.operationMessage = installation.message;
            viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
            viewState_.playEnabled = true;
            redraw();
            return;
        }
        if (refreshBeforePlay_) {
            refreshBeforePlay_ = false;
            // Only clicking Play starts downloads and Minecraft preparation.
            beginPlay();
            return;
        }
        // Opening the screen only checks the local state and the server. Downloads and
        // sync are reserved for an explicit Play click.
        viewState_.operation = installation.success ? LauncherOperation::ReadyToPlay : LauncherOperation::NeedsInstallation;
        viewState_.headline = installation.success ? L"Pronto para jogar" : L"Pronto para instalar";
        viewState_.operationMessage = installation.success
            ? L"Os arquivos do grupo são conferidos a cada Jogar."
            : L"Ao jogar, o launcher prepara Java, Minecraft e Forge.";
        viewState_.primaryActionLabel = installation.success ? L"JOGAR" : L"INSTALAR E JOGAR";
        viewState_.playEnabled = true;
    } else {
        refreshBeforePlay_ = false;
        if (installation.success) {
            showOfflineState(L"Dá para jogar com os arquivos que você já tem. Novidades do servidor chegam quando ele voltar.");
        } else {
            serverOffline_ = true;
            serverCard.value = L"Fora do ar";
            serverCard.detail = response.errorMessage.empty() ? L"Não foi possível obter o manifesto" : response.errorMessage;
            serverCard.tone = StatusTone::Error;
            viewState_.operation = LauncherOperation::ServerUnavailable;
            viewState_.headline = L"Servidor indisponível";
            viewState_.operationMessage = installation.issue == MinecraftLaunchIssue::InvalidConfiguration ? installation.message :
                L"A primeira instalação precisa do servidor. O launcher tenta de novo a cada minuto.";
            viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
            viewState_.playEnabled = true;
            if (window_) SetTimer(window_, ServerRetryTimerId, 60 * 1000, nullptr);
        }
    }
    redraw();
}

void MainPage::closeInstallation()
{
    if (!installation_.visible || installation_.running) return;
    installation_.visible = false;
    redraw();
    if (const auto* play = findControl(PlayButtonId); play && play->window && page_ == LauncherPage::Home) SetFocus(play->window);
}

void MainPage::cancelInstallation()
{
    if (!installation_.running || installation_.cancelling) return;
    installation_.cancelling = true;
    installationThread_.request_stop();
    viewState_.headline = L"Cancelando instalação...";
    viewState_.operationMessage = L"Aguardando as tarefas encerrarem com segurança...";
    viewState_.primaryActionLabel = L"CANCELANDO";
    redraw();
}

void MainPage::consumeInstallationProgress()
{
    auto snapshot = installationMailbox_.take();
    if (!snapshot) {
        if (installation_.running && !installation_.cancelling && installation_.progress.activity == InstallationActivity::Downloading) {
            installation_.meter.sample(installation_.progress.receivedBytes, InstallationTransferMeter::Clock::now());
            redraw();
        }
        return;
    }
    if (snapshot->attempt != installation_.attempt || !installation_.running) return;
    if (installation_.apply(snapshot->attempt, snapshot->progress, InstallationTransferMeter::Clock::now())) {
        installation_.steps = snapshot->steps;
        viewState_.headline = installationStageLabel(snapshot->progress.stage);
        viewState_.operationMessage = snapshot->progress.message;
        viewState_.progressPercent = snapshot->progress.activity == InstallationActivity::Downloading ? snapshot->progress.percent.value_or(0) : 0;
    }
    if (snapshot->result && installation_.finish(snapshot->attempt, *snapshot->result)) {
        // The worker has published its terminal result; joining here only completes cleanup.
        if (installationThread_.joinable()) installationThread_.join();
        KillTimer(window_, InstallationProgressTimerId);
        if (closing_) { SendMessageW(window_, WM_CLOSE, 0, 0); return; }
        viewState_.progressVisible = false;
        viewState_.progressPercent = 0;
        const auto result = *installation_.result;
        const bool repair = repairOnly_;
        repairOnly_ = false;
        if (result.outcome == InstallationOutcome::Success && repair) {
            installation_.visible = false;
            updateInstallationState(minecraftLauncherService_->validateInstallation());
            refreshMods();
            viewState_.operation = LauncherOperation::ReadyToPlay;
            viewState_.headline = L"Instalação reparada";
            viewState_.operationMessage = L"Tudo conferido. Clique em Jogar quando quiser.";
            viewState_.primaryActionLabel = L"JOGAR";
            viewState_.playEnabled = true;
            helpMessage_ = L"Reparo concluído: Java, Minecraft, Forge e mods conferidos.";
            helpMessageError_ = false;
        } else if (result.outcome == InstallationOutcome::Success) {
            launchPreparedMinecraft(result);
        } else if (!playingVanilla_ && (result.failure == InstallationFailure::Network || result.failure == InstallationFailure::Http) &&
                   minecraftLauncherService_->validateInstallation().success) {
            // The server dropped during the check, but the game on disk is complete: offer it.
            installation_.visible = false;
            showOfflineState(L"Não deu para falar com o servidor agora. Dá para jogar com os arquivos que você já tem.");
        } else {
            const bool cancelled = result.outcome == InstallationOutcome::Cancelled;
            viewState_.operation = cancelled ? LauncherOperation::NeedsInstallation : LauncherOperation::LaunchError;
            viewState_.headline = cancelled ? L"Instalação cancelada" : L"Falha na preparação";
            viewState_.operationMessage = cancelled ? L"Os arquivos completos serão reaproveitados." : result.message;
            viewState_.primaryActionLabel = cancelled ? L"CONTINUAR" : L"TENTAR NOVAMENTE";
            viewState_.playEnabled = true;
            updateInstallationState(minecraftLauncherService_->validateInstallation());
            // The screen stays with the result; its main action takes the focus.
            redraw();
            if (const auto* retry = findControl(InstallationRetryButtonId); retry && retry->window && page_ == LauncherPage::Home)
                SetFocus(retry->window);
        }
    }
    redraw();
}

void MainPage::launchVanilla()
{
    viewState_.operation = LauncherOperation::LaunchingMinecraft;
    viewState_.headline = L"Iniciando Minecraft";
    viewState_.operationMessage = L"Preparação concluída; iniciando a versão 1.8.9...";
    viewState_.primaryActionLabel = L"INICIANDO";
    viewState_.playEnabled = false;
    redraw();
    UpdateWindow(window_);
    const auto launched = minecraftLauncherService_->launchVanilla(pendingIdentity_, VanillaMinecraftService::javaExecutable(),
        VanillaMinecraftService::gameDirectory());
    if (!launched.success) {
        installation_.result = InstallationResult{InstallationOutcome::Failed, InstallationFailure::Process,
            L"Não foi possível iniciar o jogo. Tente novamente.", launched.message, 0};
        installation_.steps[5] = InstallationStepState::Failed;
        viewState_.operation = LauncherOperation::LaunchError;
        viewState_.headline = L"Não foi possível iniciar";
        viewState_.operationMessage = installation_.result->message;
        viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
        viewState_.playEnabled = true;
        return;
    }
    installation_.visible = false;
    viewState_.operation = LauncherOperation::MinecraftRunning;
    viewState_.headline = L"Jogando agora";
    // The ranking is about the group's server, so this version never reaches the counters.
    viewState_.operationMessage = originalMode_
        ? L"Minecraft 1.8.9 iniciado, sem mods. Este tempo não conta no ranking."
        : L"Minecraft 1.8.9 iniciado, sem mods. Sem conta original, só entra em servidores que aceitam nickname.";
    viewState_.primaryActionLabel = L"JOGANDO";
    SetTimer(window_, GameProcessTimerId, 1000, nullptr);
    showGameTrayIcon();
    if (settings_.minimizeOnLaunch) hideToTray(false);
}

void MainPage::launchPreparedMinecraft(const InstallationResult& result, bool offline)
{
    if (playingVanilla_) { launchVanilla(); return; }
    updateInstallationState({true, L"Instalação pronta."});
    if (!offline) {
        auto& modsCard = viewState_.statusCards[1];
        modsCard.value = L"Sincronizados";
        modsCard.detail = std::to_wstring(result.synchronizedFiles) + L" arquivos conferidos";
        modsCard.tone = StatusTone::Success;
    }
    refreshMods();
    viewState_.operation = LauncherOperation::LaunchingMinecraft;
    viewState_.headline = L"Iniciando Minecraft";
    viewState_.operationMessage = L"Preparação concluída; iniciando Forge " + std::wstring(LauncherConfig::ForgeVersion) + L"...";
    viewState_.primaryActionLabel = L"INICIANDO";
    viewState_.playEnabled = false;
    redraw();
    UpdateWindow(window_);
    const auto launched = minecraftLauncherService_->launch(pendingIdentity_);
    if (!launched.success) {
        InstallationCoordinator::recordLaunchFailure(settings_, launched.message);
        installation_.result = InstallationResult{InstallationOutcome::Failed, InstallationFailure::Process,
            L"Não foi possível iniciar o jogo. Tente novamente ou abra o log.", launched.message, 0};
        installation_.steps[5] = InstallationStepState::Failed;
        viewState_.operation = LauncherOperation::LaunchError;
        viewState_.headline = L"Não foi possível iniciar";
        viewState_.operationMessage = installation_.result->message;
        viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
        viewState_.playEnabled = true;
        return;
    }
    installation_.visible = false;
    viewState_.operation = LauncherOperation::MinecraftRunning;
    viewState_.headline = L"Jogando agora";
    viewState_.operationMessage = offline
        ? L"Jogando com os arquivos locais, sem sincronizar com o servidor. O tempo de jogo continua sendo contado."
        : L"Forge " + std::wstring(LauncherConfig::ForgeVersion) + L" iniciado para " + pendingNickname_ +
          L". O tempo de jogo é contado mesmo com a janela fechada.";
    viewState_.primaryActionLabel = L"JOGANDO";
    SetTimer(window_, GameProcessTimerId, 1000, nullptr);
    startPlaySession();
    if (settings_.minimizeOnLaunch) hideToTray(false);
}

void MainPage::updateInstallationState(const MinecraftLaunchResult& installation)
{
    auto& installationCard = viewState_.statusCards[0];
    if (installation.success) {
        installationCard.value = L"Pronta";
        installationCard.detail = L"Minecraft " + std::wstring(LauncherConfig::MinecraftVersion) + L" + Forge";
        installationCard.tone = StatusTone::Success;
        return;
    }
    installationCard.value = L"Incompleta";
    switch (installation.issue) {
    case MinecraftLaunchIssue::InvalidConfiguration: installationCard.detail = L"Confira Java e preferências"; break;
    case MinecraftLaunchIssue::JavaMissing: installationCard.detail = L"Java 17 será baixado ao jogar"; break;
    case MinecraftLaunchIssue::NativeLibrariesMissing: installationCard.detail = L"Bibliotecas nativas ausentes"; break;
    default: installationCard.detail = L"Minecraft/Forge serão baixados"; break;
    }
    installationCard.tone = StatusTone::Warning;
}

// ---------------------------------------------------------------------------------------------
// Play time and tray
// ---------------------------------------------------------------------------------------------

void MainPage::startPlaySession()
{
    playtime_ = PlaytimeStore::load(LauncherSettingsStore::dataDirectory());
    // The counter as it was before this session is what a first import may bring over.
    startPlaytimeSync(playtime_);
    playSession_.start(playtime_, PlaySession::Clock::now(), wallClock());
    flushPlaytime();
    SetTimer(window_, PlaytimeTimerId, 60 * 1000, nullptr);
    showGameTrayIcon();
    updateTrayTooltip();
}

void MainPage::showGameTrayIcon()
{
    const int size = GetSystemMetrics(SM_CXSMICON);
    const auto icon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(101), IMAGE_ICON, size, size, LR_SHARED));
    trayIcon_.show(window_, TrayIconMessage, icon ? icon : LoadIcon(nullptr, IDI_APPLICATION), L"Eternal Launcher · Jogando");
}

void MainPage::flushPlaytime()
{
    if (!playSession_.active()) return;
    playtime_ = playSession_.snapshot(PlaySession::Clock::now(), wallClock());
    (void)PlaytimeStore::save(LauncherSettingsStore::dataDirectory(), playtime_);
}

void MainPage::finishPlaySession()
{
    if (playSession_.active()) {
        flushPlaytime();
        playSession_.stop();
    }
    if (window_) KillTimer(window_, PlaytimeTimerId);
    playtimeSyncThread_.request_stop();
    trayIcon_.hide();
    hiddenToTray_ = false;
}

void MainPage::updateTrayTooltip()
{
    if (!playSession_.active()) return;
    const auto elapsed = playSession_.elapsedSeconds(PlaySession::Clock::now());
    trayIcon_.setTooltip(L"Eternal Launcher · Jogando · " + PlaytimeStore::formatDuration(elapsed));
}

void MainPage::hideToTray(bool closedByUser)
{
    if (!trayIcon_.visible()) {
        // Without an icon there is no way back; minimize instead of hiding.
        ShowWindow(window_, SW_MINIMIZE);
        return;
    }
    ShowWindow(window_, SW_HIDE);
    hiddenToTray_ = true;
    if (closedByUser) exitAfterGame_ = true;
    if (!trayNoticeShown_) {
        trayNoticeShown_ = true;
        trayIcon_.notify(L"O Eternal continua contando seu tempo",
            closedByUser ? L"Ele fecha sozinho quando o Minecraft encerrar. Clique no ícone para abrir." :
                           L"Clique no ícone perto do relógio para abrir o launcher.");
    }
}

void MainPage::restoreFromTray()
{
    ShowWindow(window_, IsIconic(window_) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(window_);
    hiddenToTray_ = false;
    exitAfterGame_ = false;
    redraw();
}

void MainPage::showTrayMenu(POINT anchor)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    if (playSession_.active()) {
        const auto status = L"Jogando · " + PlaytimeStore::formatDuration(playSession_.elapsedSeconds(PlaySession::Clock::now()));
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, status.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(menu, MF_STRING, MenuTrayOpen, L"Abrir Eternal Launcher");
    AppendMenuW(menu, MF_STRING, MenuTrayExit, L"Sair do launcher");
    SetMenuDefaultItem(menu, MenuTrayOpen, FALSE);
    // Required so the menu closes when the user clicks elsewhere.
    SetForegroundWindow(window_);
    const UINT alignment = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    TrackPopupMenuEx(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | alignment, anchor.x, anchor.y, window_, nullptr);
    PostMessageW(window_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

// ---------------------------------------------------------------------------------------------
// Previews
// ---------------------------------------------------------------------------------------------

bool MainPage::renderPreviews(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    UINT count = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    if (!size) return false;
    std::vector<BYTE> bytes(size);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());
    if (Gdiplus::GetImageEncoders(count, size, encoders) != Gdiplus::Ok) return false;
    CLSID png{};
    bool found = false;
    for (UINT i = 0; i < count; ++i) if (std::wstring_view(encoders[i].MimeType) == L"image/png") {
        png = encoders[i].Clsid; found = true; break;
    }
    if (!found) return false;

    const auto save = [&](const wchar_t* filename, float scale, float width = WindowWidth, float height = WindowHeight) {
        viewScale_ = scale;
        clientWidth_ = std::round(width * scale);
        clientHeight_ = std::round(height * scale);
        offsetX_ = std::floor((clientWidth_ - WindowWidth * scale) / 2);
        offsetY_ = std::floor((clientHeight_ - WindowHeight * scale) / 2);
        displayedProgress_ = static_cast<float>(viewState_.progressPercent);
        animationPhase_ = 0.45F;
        Gdiplus::Bitmap frame(static_cast<INT>(clientWidth_), static_cast<INT>(clientHeight_), PixelFormat32bppPARGB);
        Gdiplus::Graphics graphics(&frame);
        render(graphics, clientWidth_, clientHeight_, true);
        return frame.Save((directory / filename).c_str(), &png) == Gdiplus::Ok;
    };

    const auto initial = viewState_;
    const auto ready = [&] {
        viewState_ = initial;
        viewState_.statusCards = {
            {L"INSTALAÇÃO", L"Pronta", L"Minecraft 1.20.1 + Forge", StatusTone::Success},
            {L"MODS", L"Sincronizados", L"184 arquivos conferidos", StatusTone::Success},
            {L"SERVIDOR", L"Online", L"Respondeu em 84 ms", StatusTone::Success}};
        viewState_.operation = LauncherOperation::ReadyToPlay;
        viewState_.headline = L"Pronto para jogar";
        viewState_.operationMessage = L"Os arquivos do grupo são conferidos a cada Jogar.";
        viewState_.primaryActionLabel = L"JOGAR";
        viewState_.playEnabled = true;
        viewState_.news = {
            {L"EVENTO", L"Noite de construção no spawn", L"Sábado às 20h: tragam blocos e ideias para a nova vila.", L"25 out 2026",
             L"https://example.com/evento", false},
            {L"SERVIDOR", L"Backup automático a cada 6 horas", L"Os mundos agora têm cópias de segurança frequentes.", L"20 out 2026", L"", false},
            NewsFeed::releaseNotes()};
        mods_ = {};
        mods_.manifestKnown = true;
        for (const auto* name : {L"create-1.20.1-0.5.1.f.jar", L"jei-1.20.1-forge-15.3.0.4.jar", L"sophisticatedbackpacks-1.20.1-3.20.5.jar",
                 L"waystones-forge-1.20.1-14.1.3.jar", L"farmersdelight-1.20.1-1.2.4.jar", L"supplementaries-1.20-2.8.10.jar",
                 L"moonlight-1.20-2.11.30-forge.jar", L"balm-forge-1.20.1-7.2.2.jar", L"cloth-config-11.1.118-forge.jar",
                 L"architectury-9.2.14-forge.jar", L"geckolib-forge-1.20.1-4.4.4.jar", L"curios-forge-5.9.1+1.20.1.jar"})
            mods_.server.push_back({name, 2'400'000, true});
        mods_.user = {{L"journeymap-1.20.1-5.9.18-forge.jar", {}, 6'800'000, true},
                      {L"Xaeros_Minimap_24.2.0_Forge_1.20.jar", {}, 1'200'000, false}};
        modsScanned_ = true;
        syncModKnobs();
        playtime_ = {4 * 3600 + 12 * 60, 3600, wallClock() - 26 * 3600};
        originalMode_ = false; updatingLauncher_ = false; updateWarning_.clear();
        installation_ = {};
        serverOffline_ = false;
        screenshots_.clear(); screenshotThumbnails_.clear(); screenshotRow_ = 0; screenshotViewer_ = -1;
        viewerImage_.reset(); viewerImagePath_.clear(); screenshotsMessage_.clear();
        previewRankingHours_ = {312, 167, 96};
        celebration_ = 0; previewTier_ = 0;
        levelsOpen_ = false; previewLevelsSeconds_ = 0;
        loginOpen_ = false; loginError_.clear(); loginNotice_.clear(); previewEternalName_ = L"Steve"; previewLoginUser_.clear();
        skinLibrary_.clear(); currentSkin_.reset(); skinSelected_ = 0; skinRow_ = 0; skinsMessage_.clear();
        modpackChangesVisible_ = false; modpackChanges_ = {};
        page_ = LauncherPage::Home;
        newsFirst_ = 0; newsLoading_ = false; newsError_.clear();
        modsMessage_.clear(); helpMessage_.clear();
    };

    // Home.
    viewState_ = initial; page_ = LauncherPage::Home; modsScanned_ = false; mods_ = {};
    if (!save(L"loading.png", 1)) return false;
    ready();
    if (!save(L"desktop.png", 1)) return false;
    if (!save(L"compact.png", .8F)) return false;
    if (!save(L"wide.png", 1, 1600, 800)) return false;
    // Unread news: spotlight, NOVO tags and the rail badge.
    viewState_.news[0].unread = true; viewState_.news[1].unread = true;
    if (!save(L"spotlight.png", 1)) return false;
    ready(); viewState_.news.back().unread = true;
    if (!save(L"spotlight-update.png", 1)) return false;
    ready(); page_ = LauncherPage::News;
    viewState_.news[0].unread = true; viewState_.news[1].unread = true;
    if (!save(L"news-unread.png", 1)) return false;
    // Conta Eternal: signed out, and the sign-in panel.
    ready(); previewEternalName_.clear();
    if (!save(L"eternal-signed-out.png", 1)) return false;
    loginOpen_ = true; previewLoginUser_ = L"Steve";
    if (!save(L"login.png", 1)) return false;
    loginError_ = L"Usuário ou senha incorretos.";
    if (!save(L"login-error.png", 1)) return false;
    loginError_.clear(); loginMode_ = 1; loginFirstName_ = L"Nippw";
    if (!save(L"login-first-access.png", 1)) return false;
    loginMode_ = 2; loginError_ = L"Esse nick já tem conta. Se é seu, use Entrar.";
    if (!save(L"login-register.png", 1)) return false;
    loginMode_ = 0; loginError_.clear();
    // Skins page with generated skins.
    {
        const auto makeSkin = [](int seed, bool slim) {
            SkinTexture skin;
            skin.pixels.assign(64 * 64, 0);
            const auto put = [&skin](int x0, int y0, int w, int h, std::uint32_t color, int noise) {
                for (int y = y0; y < y0 + h; ++y)
                    for (int x = x0; x < x0 + w; ++x) {
                        const int shade = ((x * 31 + y * 17) % 3 - 1) * noise;
                        const auto channel = [&](int shift) { return static_cast<std::uint32_t>(std::clamp(static_cast<int>((color >> shift) & 0xFF) + shade, 0, 255)); };
                        skin.pixels[static_cast<std::size_t>(y * 64 + x)] = 0xFF000000u | channel(16) << 16 | channel(8) << 8 | channel(0);
                    }
            };
            const std::uint32_t shirts[] = {0x3A7BD5, 0xC0392B, 0x27AE60, 0x8E44AD, 0xE67E22, 0x16A085};
            const std::uint32_t hair[] = {0x3B2314, 0xD4A017, 0x1C1C1C, 0x8B4513, 0xC0C0C0, 0x5B3A29};
            const auto shirt = shirts[seed % 6], skinTone = 0xC69C6Du, pants = 0x2C3E70u;
            put(0, 8, 32, 8, skinTone, 6); put(8, 0, 16, 8, hair[seed % 6], 8); put(0, 8, 32, 2, hair[seed % 6], 8);  // head
            put(8, 12, 2, 1, 0xFFFFFF, 0); put(9, 12, 1, 1, 0x2244AA, 0); put(13, 12, 2, 1, 0xFFFFFF, 0); put(13, 12, 1, 1, 0x2244AA, 0);
            put(11, 14, 2, 1, 0x8B5A3C, 0);
            put(16, 20, 24, 12, shirt, 10); put(16, 16, 24, 4, shirt, 10);   // body
            put(40, 16, 16, 16, shirt, 10); put(32, 48, 16, 16, shirt, 10);   // arms
            put(44, 28, slim ? 3 : 4, 4, skinTone, 4); put(36, 60, slim ? 3 : 4, 4, skinTone, 4);
            if (slim) for (int y = 16; y < 32; ++y) { skin.pixels[static_cast<std::size_t>(y * 64 + 54)] = 0; skin.pixels[static_cast<std::size_t>(y * 64 + 55)] = 0; }
            put(0, 16, 16, 16, pants, 6); put(16, 48, 16, 16, pants, 6);    // legs
            put(0, 28, 16, 4, 0x3D3D3D, 4); put(16, 60, 16, 4, 0x3D3D3D, 4); // shoes
            return skin;
        };
        ready(); page_ = LauncherPage::Skins;
        currentSkin_ = makeSkin(0, false); currentSkinKnown_ = true; currentSkinSlim_ = false;
        const wchar_t* names[] = {L"Cavaleiro", L"Exploradora", L"Mineiro", L"Mago", L"Ferreira"};
        for (int i = 0; i < 5; ++i) skinLibrary_.push_back({names[i], L"C:\\preview\\skins\\x.png", "", makeSkin(i + 1, i % 2 == 1), i % 2 == 1});
        skinSelected_ = 2; skinSlimChoice_ = true;
        if (!save(L"skins.png", 1)) return false;
        // Explorar: three players' skins, then shared ones.
        skinExplore_ = true;
        const wchar_t* players[] = {L"Aurora_", L"zNippw", L"Rafa"};
        for (int i = 0; i < 3; ++i) exploreGroup_.push_back({{}, players[i], players[i], true, i == 0, "", makeSkin(i + 7, i == 0)});
        const wchar_t* shared[][2] = {{L"Pirata", L"Rafa"}, {L"Astronauta", L"Aurora_"}, {L"Ninja da Lua", L"Steve"}, {L"Fazendeira", L"Lia"},
                                      {L"Viking", L"zNippw"}, {L"Cientista", L"Rafa"}};
        for (int i = 0; i < 6; ++i)
            exploreCommunity_.push_back({L"00000000000000a" + std::to_wstring(i), shared[i][0], shared[i][1], false, i % 3 == 1, "", makeSkin(i + 11, i % 3 == 1)});
        exploreSelected_ = 4; skinSlimChoice_ = true;
        if (!save(L"skins-explore.png", 1)) return false;
        exploreSelected_ = 1; skinSlimChoice_ = false;
        if (!save(L"skins-explore-player.png", 1)) return false;
        exploreSearch_ = ExploreSkin{{}, L"Technoblade", L"Conta original do Minecraft", false, false, "", makeSkin(20, false), true};
        exploreSelected_ = 0; skinSlimChoice_ = false; previewSkinSearch_ = L"Technoblade";
        skinsMessage_ = L"Skin de Technoblade encontrada. Use ou salve na sua biblioteca.";
        if (!save(L"skins-explore-search.png", 1)) return false;
        exploreSearch_.reset(); previewSkinSearch_.clear(); skinsMessage_.clear();
        exploreGroup_.clear(); exploreCommunity_.clear(); exploreSelected_ = 0; exploreLoading_ = true;
        if (!save(L"skins-explore-loading.png", 1)) return false;
        exploreLoading_ = false;
        if (!save(L"skins-explore-empty.png", 1)) return false;
        skinExplore_ = false;
        skinSelected_ = 0; skinSlimChoice_ = false; previewEternalName_.clear(); skinLibrary_.clear(); currentSkin_.reset(); currentSkinKnown_ = false;
        if (!save(L"skins-signed-out.png", 1)) return false;
    }
    // The modpack changed since the last visit.
    ready();
    modpackChanges_ = ModpackChangelog::compare(
        {L"create-1.20.1-0.5.1.f.jar", L"jei-1.20.1-forge-15.3.0.4.jar", L"waystones-forge-1.20.1-14.1.3.jar", L"oldmap-1.0.jar",
         L"farmersdelight-1.20.1-1.2.4.jar", L"geckolib-forge-1.20.1-4.4.4.jar"},
        {L"create-1.20.1-0.5.2.jar", L"jei-1.20.1-forge-15.3.0.8.jar", L"waystones-forge-1.20.1-14.1.3.jar", L"aquaculture-1.20.1-2.5.1.jar",
         L"sophisticated_backpacks-1.20.1-3.20.5.jar", L"supplementaries-1.20-2.8.10.jar", L"farmersdelight-1.20.1-1.2.4.jar",
         L"geckolib-forge-1.20.1-4.4.9.jar", L"comforts-forge-6.4.0+1.20.1.jar"});
    modpackChangesVisible_ = true;
    if (!save(L"modpack-changes.png", 1)) return false;
    // A play-time level reached, and the level on the player card.
    ready(); celebration_ = 3; celebrationSeconds_ = 96 * 3600 + 20 * 60; previewTier_ = 3;
    if (!save(L"achievement.png", 1)) return false;
    celebration_ = 5; celebrationSeconds_ = 312 * 3600;
    if (!save(L"achievement-max.png", 1)) return false;
    celebration_ = 0;
    if (!save(L"player-tier.png", 1)) return false;
    // Every level, opened from Tempo jogado.
    levelsOpen_ = true; previewLevelsSeconds_ = 96 * 3600 + 20 * 60;
    if (!save(L"levels.png", 1)) return false;
    previewLevelsSeconds_ = 4 * 3600; previewTier_ = 1;
    if (!save(L"levels-start.png", 1)) return false;
    previewLevelsSeconds_ = 330 * 3600; previewTier_ = 5;
    if (!save(L"levels-max.png", 1)) return false;
    previewLevelsSeconds_ = 0; previewTier_ = 0;
    if (!save(L"levels-unknown.png", 1)) return false;
    levelsOpen_ = false;
    // Podium at the first play-time levels.
    ready(); previewRankingHours_ = {60, 30, 8};
    if (!save(L"ranking-early.png", 1)) return false;
    // Every player: the card scrolls.
    ready(); previewExtraPlayers_ = 9;
    if (!save(L"ranking-all.png", 1)) return false;
    previewRankingScroll_ = 4 * HomeView::RankingRowHeight;
    if (!save(L"ranking-scrolled.png", 1)) return false;
    previewExtraPlayers_ = 0; previewRankingScroll_ = 0;
    ready();
    ready(); originalMode_ = true;
    if (!save(L"original.png", 1)) return false;
    ready(); vanillaSelected_ = true;
    if (!save(L"version-189.png", 1)) return false;
    vanillaSelected_ = false;
    ready();
    viewState_.operation = LauncherOperation::ServerUnavailable;
    viewState_.headline = L"Servidor indisponível";
    viewState_.operationMessage = L"Não foi possível consultar o manifesto. Verifique sua conexão e tente novamente.";
    viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
    viewState_.statusCards[2] = {L"SERVIDOR", L"Indisponível", L"Falha ao consultar o manifesto", StatusTone::Error};
    if (!save(L"error.png", 1)) return false;
    ready();
    showOfflineState(L"Dá para jogar com os arquivos que você já tem. Novidades do servidor chegam quando ele voltar.");
    if (!save(L"server-offline.png", 1)) return false;
    ready();
    viewState_.operation = LauncherOperation::NeedsInstallation;
    viewState_.headline = L"Pronto para instalar";
    viewState_.operationMessage = L"Ao jogar, o launcher prepara Java, Minecraft e Forge.";
    viewState_.primaryActionLabel = L"INSTALAR E JOGAR";
    viewState_.statusCards[0] = {L"INSTALAÇÃO", L"Incompleta", L"Java 17 será baixado ao jogar", StatusTone::Warning};
    playtime_ = {};
    if (!save(L"first-install.png", 1)) return false;
    ready();
    viewState_.operation = LauncherOperation::InvalidNickname;
    viewState_.headline = L"Confira seu nickname";
    viewState_.operationMessage = L"Use de 3 a 16 letras, números ou underline antes de jogar.";
    viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
    if (!save(L"nickname-error.png", 1)) return false;
    ready(); originalMode_ = true;
    viewState_.operation = LauncherOperation::SigningIn;
    viewState_.headline = L"Entrando com Microsoft";
    viewState_.operationMessage = L"Aguarde o código de confirmação...";
    viewState_.primaryActionLabel = L"CONECTANDO";
    viewState_.playEnabled = false;
    if (!save(L"sign-in.png", 1)) return false;
    ready();
    viewState_.operation = LauncherOperation::MinecraftRunning;
    viewState_.headline = L"Jogando agora";
    viewState_.operationMessage = L"Forge 47.4.10 iniciado para Steve. O tempo de jogo é contado mesmo com a janela fechada.";
    viewState_.primaryActionLabel = L"JOGANDO";
    viewState_.playEnabled = false;
    if (!save(L"playing.png", 1)) return false;

    // Launcher self-update.
    const wchar_t* updateNames[] = {L"update-check.png", L"update-download.png", L"update-apply.png", L"update-restart.png"};
    for (int phase = 0; phase < 4; ++phase) {
        ready();
        updatingLauncher_ = true;
        applyLauncherUpdateProgress({static_cast<LauncherUpdatePhase>(phase), phase == 0 ? 0 : phase == 1 ? 47 : 100});
        if (!save(updateNames[phase], 1)) return false;
    }
    ready();
    updateWarning_ = L"A atualização falhou. A versão anterior foi restaurada.";
    if (!save(L"update-recovery.png", 1)) return false;

    // Installation panel.
    for (int variant = 0; variant < 7; ++variant) {
        ready();
        installation_.begin();
        auto& value = installation_.progress;
        value.message = L"Consultando os arquivos do grupo...";
        viewState_.operation = LauncherOperation::PreparingMinecraft;
        viewState_.playEnabled = false; viewState_.progressVisible = true; viewState_.progressPercent = 0;
        viewState_.headline = L"Preparando seu mundo"; viewState_.operationMessage = value.message;
        viewState_.primaryActionLabel = L"PREPARANDO";
        const wchar_t* filename = L"installation-checking.png";
        float scale = 1;
        if (variant != 0) {
            installation_.steps[0] = InstallationStepState::Available;
            installation_.steps[1] = InstallationStepState::Active;
            value.stage = InstallationStage::Minecraft; value.activity = InstallationActivity::Downloading;
            value.message = L"Baixando Minecraft, bibliotecas e assets...";
            value.receivedBytes = 282ULL * 1024 * 1024; value.totalBytes = 600ULL * 1024 * 1024; value.percent = 47;
            value.completedFiles = 140; value.totalFiles = 900; value.reusedFiles = 620;
            const auto now = InstallationTransferMeter::Clock::now();
            installation_.meter.sample(value.receivedBytes - 12ULL * 1024 * 1024, now - std::chrono::seconds(3));
            installation_.meter.sample(value.receivedBytes, now);
            viewState_.headline = L"Minecraft e assets"; viewState_.operationMessage = value.message; viewState_.progressPercent = 47;
            filename = L"installation-download.png";
        }
        if (variant == 2) {
            value = {InstallationStage::Forge, InstallationActivity::Installing, InstallationStepState::Active, L"Executando instalador Forge 47.4.10..."};
            installation_.steps[1] = InstallationStepState::Complete; installation_.steps[2] = InstallationStepState::Complete;
            installation_.steps[3] = InstallationStepState::Active;
            viewState_.headline = L"Forge"; viewState_.operationMessage = value.message; viewState_.progressPercent = 0;
            filename = L"installation-forge.png";
        } else if (variant == 3 || variant == 4) {
            const bool cancelled = variant == 4;
            installation_.finish(installation_.attempt, {cancelled ? InstallationOutcome::Cancelled : InstallationOutcome::Failed,
                cancelled ? InstallationFailure::None : InstallationFailure::Network,
                cancelled ? L"Instalação cancelada. Os arquivos completos serão reaproveitados." :
                            L"A conexão foi interrompida. Confira sua internet e tente novamente."});
            viewState_.operation = cancelled ? LauncherOperation::NeedsInstallation : LauncherOperation::LaunchError;
            viewState_.headline = cancelled ? L"Instalação cancelada" : L"Falha na preparação";
            viewState_.operationMessage = installation_.result->message; viewState_.playEnabled = true;
            viewState_.progressVisible = false; viewState_.progressPercent = 0;
            viewState_.primaryActionLabel = cancelled ? L"CONTINUAR" : L"TENTAR NOVAMENTE";
            filename = cancelled ? L"installation-cancelled.png" : L"installation-error.png";
        } else if (variant == 5) {
            installation_.cancelling = true;
            viewState_.headline = L"Cancelando instalação..."; viewState_.operationMessage = L"Aguardando as tarefas encerrarem com segurança...";
            viewState_.primaryActionLabel = L"CANCELANDO"; viewState_.progressPercent = 0;
            filename = L"installation-cancelling.png";
        } else if (variant == 6) {
            filename = L"installation-compact.png";
            scale = .8F;
        }
        if (!save(filename, scale)) return false;
    }

    // Other pages.
    ready(); page_ = LauncherPage::Versions;
    if (!save(L"versions.png", 1)) return false;
    vanillaSelected_ = true;
    if (!save(L"versions-189.png", 1)) return false;
    if (!save(L"versions-compact.png", .8F)) return false;
    vanillaSelected_ = false;
    ready(); page_ = LauncherPage::News;
    if (!save(L"news.png", 1)) return false;
    viewState_.news.clear();
    if (!save(L"news-empty.png", 1)) return false;
    ready(); page_ = LauncherPage::Mods;
    if (!save(L"mods.png", 1)) return false;
    mods_.user.clear();
    if (!save(L"mods-empty.png", 1)) return false;
    // Screenshots: generated blocky landscapes stand in for real captures.
    {
        const auto scene = [](int seed, int width, int height) {
            auto bitmap = std::make_shared<Gdiplus::Bitmap>(width, height, PixelFormat32bppPARGB);
            Gdiplus::Graphics g(bitmap.get());
            const BYTE skies[][6] = {{86, 148, 232, 186, 214, 250}, {38, 30, 82, 214, 120, 120}, {14, 18, 46, 70, 52, 120},
                                     {120, 190, 240, 230, 240, 250}, {60, 40, 90, 250, 170, 110}};
            const auto& sky = skies[seed % 5];
            Gdiplus::LinearGradientBrush fill(Gdiplus::PointF(0, 0), Gdiplus::PointF(0, static_cast<float>(height)),
                Gdiplus::Color(255, sky[0], sky[1], sky[2]), Gdiplus::Color(255, sky[3], sky[4], sky[5]));
            g.FillRectangle(&fill, 0, 0, width, height);
            const float cell = static_cast<float>(width) / 30;
            for (int column = 0; column < 31; ++column) {
                const int ground = 11 + static_cast<int>(3 * std::sin(column * .35F + seed) + 2 * std::sin(column * .9F + seed * 2.3F));
                for (int row = ground; row < 20; ++row) {
                    const int shade = (column * 73 + row * 151 + seed * 31) % 5 * 6 - 12;
                    const auto channel = [shade](int value) { return static_cast<BYTE>(std::clamp(value + shade, 0, 255)); };
                    const bool top = row == ground;
                    Gdiplus::SolidBrush block(top ? Gdiplus::Color(255, channel(84), channel(170), channel(88)) :
                        row < ground + 3 ? Gdiplus::Color(255, channel(130), channel(92), channel(64)) :
                                           Gdiplus::Color(255, channel(110), channel(112), channel(122)));
                    g.FillRectangle(&block, static_cast<float>(column) * cell, static_cast<float>(row) * cell, cell + .5F, cell + .5F);
                }
                if ((column * 7 + seed) % 9 == 0) {
                    // A tree.
                    Gdiplus::SolidBrush trunk(Gdiplus::Color(255, 104, 74, 46)), leaves(Gdiplus::Color(255, 52, 128, 60));
                    g.FillRectangle(&trunk, static_cast<float>(column) * cell, static_cast<float>(ground - 3) * cell, cell, 3 * cell);
                    g.FillRectangle(&leaves, static_cast<float>(column - 1) * cell, static_cast<float>(ground - 6) * cell, 3 * cell, 3 * cell);
                }
            }
            return bitmap;
        };
        ready(); page_ = LauncherPage::Screenshots;
        const auto now = wallClock();
        for (int i = 0; i < 18; ++i) {
            ScreenshotEntry entry;
            entry.path = L"C:\\preview\\screenshots\\2026-11-" + std::to_wstring(13 - i / 3) + L"_2" + std::to_wstring(i % 3) + L".14.0" +
                std::to_wstring(i % 10) + L".png";
            entry.size = 1'800'000 + static_cast<std::uintmax_t>(i) * 137'000;
            entry.modified = now - i * 7 * 3600;
            entry.vanilla = i == 4 || i == 9;
            screenshots_.push_back(entry);
            if (i != 7) screenshotThumbnails_[entry.path.wstring()] = scene(i, 416, 234);  // one still loading
        }
        if (!save(L"screenshots.png", 1)) return false;
        if (!save(L"screenshots-compact.png", .8F)) return false;
        screenshotViewer_ = 2;
        viewerImage_ = scene(2, 1920, 1080);
        viewerImagePath_ = screenshots_[2].path.wstring();
        viewerWidth_ = 1920; viewerHeight_ = 1080;
        if (!save(L"screenshot-viewer.png", 1)) return false;
        screenshotsMessage_ = L"Copiada! Cole no Discord ou no WhatsApp com Ctrl+V.";
        if (!save(L"screenshot-viewer-copied.png", 1)) return false;
        // Already in the group gallery.
        const auto sharedBefore = galleryShared_;
        galleryShared_.add({L"00000000000000aa", "preview", GallerySharedStore::sourceKey(screenshots_[2].path, screenshots_[2].size)});
        galleryShared_.add({L"00000000000000bb", "preview", GallerySharedStore::sourceKey(screenshots_[5].path, screenshots_[5].size)});
        screenshotsMessage_ = L"Compartilhada! Já está na Galeria do grupo.";
        if (!save(L"screenshot-viewer-shared.png", 1)) return false;
        screenshotViewer_ = -1; screenshotsMessage_.clear();
        if (!save(L"screenshots-shared.png", 1)) return false;

        // The group gallery tab.
        galleryTab_ = true; galleryLoaded_ = true;
        const wchar_t* names[] = {L"Aurora_", L"Pedrinho", L"Steve", L"LunaCraft", L"Tico_BR"};
        galleryPhotos_.clear(); galleryThumbnails_.clear();
        for (int i = 0; i < 14; ++i) {
            GalleryPhoto photo;
            wchar_t id[17]{};
            swprintf_s(id, L"%016x", 0xb0 + i);
            photo.id = id;
            photo.name = names[i % 5];
            photo.created = now - (i < 3 ? 600 + i * 2400 : i * 9 * 3600);
            photo.width = 1920; photo.height = 1080;
            galleryPhotos_.push_back(photo);
            if (i != 6) galleryThumbnails_[photo.id] = scene(i + 7, 416, 234);
        }
        galleryShared_.add({galleryPhotos_[2].id, "preview", L""});
        if (!save(L"gallery.png", 1)) return false;
        screenshotViewer_ = 1;
        galleryImage_ = scene(8, 1920, 1080); galleryImageId_ = galleryPhotos_[1].id;
        if (!save(L"gallery-viewer.png", 1)) return false;
        screenshotViewer_ = 2;
        galleryImage_ = scene(9, 1920, 1080); galleryImageId_ = galleryPhotos_[2].id;
        if (!save(L"gallery-viewer-mine.png", 1)) return false;
        screenshotViewer_ = -1; galleryPhotos_.clear(); galleryLoaded_ = false; galleryLoading_ = true;
        if (!save(L"gallery-loading.png", 1)) return false;
        galleryLoading_ = false; galleryLoaded_ = true;
        if (!save(L"gallery-empty.png", 1)) return false;
        galleryTab_ = false; galleryImage_.reset(); galleryImageId_.clear(); galleryThumbnails_.clear();
        galleryShared_ = sharedBefore;
        ready(); page_ = LauncherPage::Screenshots;
        if (!save(L"screenshots-empty.png", 1)) return false;
    }
    ready(); page_ = LauncherPage::Help;
    if (!save(L"help.png", 1)) return false;
    helpMessage_ = L"Informações copiadas. Cole no grupo ou no relato do GitHub.";
    if (!save(L"help-message.png", 1)) return false;

    // Settings.
    for (int variant = 0; variant < 6; ++variant) {
        ready(); page_ = LauncherPage::Settings;
        LauncherSettings settings;
        const wchar_t* filename = L"settings.png";
        float scale = 1;
        auto tab = SettingsPage::Tab::General;
        if (variant == 1) { filename = L"settings-compact.png"; scale = .8F; }
        if (variant == 2) { settings.maximumMemoryMegabytes = 6656; filename = L"settings-half-step.png"; }
        if (variant == 3) { settings.minimumMemoryMegabytes = 8192; tab = SettingsPage::Tab::Advanced; filename = L"settings-error.png"; }
        if (variant == 4) { tab = SettingsPage::Tab::Advanced; filename = L"settings-advanced.png"; }
        if (variant == 5) { tab = SettingsPage::Tab::About; filename = L"settings-about.png"; }
        settingsPage_.open(settings, {});
        settingsPage_.setTab(tab);
        settingsPage_.settle();
        if (variant == 3) {
            std::wstring validation;
            LauncherSettingsStore::validate(settings, validation, false);
            settingsPage_.setMessage(validation, true);
        }
        if (!save(filename, scale)) return false;
    }
    viewState_ = initial;
    page_ = LauncherPage::Home;
    originalMode_ = false;
    installation_ = {};
    screenshots_.clear(); screenshotThumbnails_.clear(); screenshotViewer_ = -1; viewerImage_.reset(); viewerImagePath_.clear();
    viewScale_ = 1;
    return true;
}
