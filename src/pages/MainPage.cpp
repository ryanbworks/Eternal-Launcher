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

    // Creation order is also the keyboard tab order.
    struct ControlSpec { int id; bool chrome; bool tabStop; const wchar_t* label; };
    constexpr ControlSpec ControlSpecs[] = {
        {NavHomeId, true, true, L"Início"},
        {NavNewsId, true, true, L"Notícias"},
        {NavModsId, true, true, L"Mods"},
        {NavSettingsId, true, true, L"Configurações"},
        {NavHelpId, true, true, L"Ajuda"},
        {NicknameInputId, false, true, nullptr},
        {EditNicknameButtonId, false, true, L"Editar nickname"},
        {OfflineButtonId, false, true, L"Nickname"},
        {OriginalButtonId, false, true, L"Conta original"},
        {UpdateDownloadButtonId, false, true, L"Baixar launcher"},
        {PlayButtonId, false, true, L"Jogar"},
        {PlayMenuButtonId, false, true, L"Mais ações"},
        {QuickFolderButtonId, false, true, L"Pasta do jogo"},
        {QuickRepairButtonId, false, true, L"Reparar"},
        {QuickLogsButtonId, false, true, L"Logs"},
        {ToastNewsButtonId, false, true, L"Ver novidades"},
        {NewsCardFirstId, false, true, L"Notícia 1"},
        {NewsCardFirstId + 1, false, true, L"Notícia 2"},
        {NewsCardFirstId + 2, false, true, L"Notícia 3"},
        {NewsAllButtonId, false, true, L"Ver todas as notícias"},
        {InstallationLogButtonId, false, true, L"Abrir log"},
        {CancelInstallationButtonId, false, true, L"Cancelar instalação"},
        {NewsLinkFirstId, false, true, L"Abrir notícia 1"},
        {NewsLinkFirstId + 1, false, true, L"Abrir notícia 2"},
        {NewsLinkFirstId + 2, false, true, L"Abrir notícia 3"},
        {NewsLinkFirstId + 3, false, true, L"Abrir notícia 4"},
        {ModsRefreshButtonId, false, true, L"Atualizar lista"},
        {ModsFolderButtonId, false, true, L"Abrir pasta"},
        {ModsAddButtonId, false, true, L"Adicionar mod"},
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

    constexpr wchar_t IssueUrl[] = L"https://github.com/ryanbworks/Eternal-Launcher/issues/new";

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
    launcherUpdateThread_.request_stop();
    if (launcherUpdateThread_.joinable()) launcherUpdateThread_.join();
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
    checkVersionToast();
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
        if (prepareFrame()) {
            Gdiplus::Graphics output(reinterpret_cast<HDC>(wParam));
            output.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            const int width = static_cast<int>(frame_->GetWidth()), height = static_cast<int>(frame_->GetHeight());
            output.DrawImage(frame_.get(), Gdiplus::Rect(0, 0, width, height), 0, 0, width, height, Gdiplus::UnitPixel);
        }
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
        const bool hovered = page_ == LauncherPage::Home && !installation_.visible &&
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
            if (notification == EN_CHANGE || notification == EN_SETFOCUS || notification == EN_KILLFOCUS) redraw();
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
        Gdiplus::Graphics output(item->hDC);
        output.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        output.DrawImage(frame_.get(), Gdiplus::Rect(0, 0, button.right - button.left, button.bottom - button.top),
            button.left, button.top, button.right - button.left, button.bottom - button.top, Gdiplus::UnitPixel);
        return TRUE;
    }
    case WM_TIMER:
        if (wParam == GameProcessTimerId) pollGameProcess();
        else if (wParam == AnimationTimerId) animate();
        else if (wParam == InstallationProgressTimerId) consumeInstallationProgress();
        else if (wParam == PlaytimeTimerId) { flushPlaytime(); updateTrayTooltip(); redraw(); }
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
            pendingNickname_ = microsoftIdentity_.name;
            redraw();
            beginPlay();
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
        trayIcon_.hide();
        fileServerThread_.request_stop();
        installationThread_.request_stop();
        microsoftAuthThread_.request_stop();
        newsThread_.request_stop();
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
        Gdiplus::Graphics output(dc);
        output.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        // Explicit pixel bounds prevent GDI+ from scaling the buffer again at 125%/150% DPI.
        const int width = static_cast<int>(frame_->GetWidth()), height = static_cast<int>(frame_->GetHeight());
        output.DrawImage(frame_.get(), Gdiplus::Rect(0, 0, width, height), 0, 0, width, height, Gdiplus::UnitPixel);
    }
    EndPaint(window_, &ps);
}

bool MainPage::prepareFrame()
{
    RECT client{};
    GetClientRect(window_, &client);
    if (client.right <= 0 || client.bottom <= 0) return false;
    if (!frame_ || frame_->GetWidth() != static_cast<UINT>(client.right) ||
        frame_->GetHeight() != static_cast<UINT>(client.bottom)) {
        frame_ = std::make_unique<Gdiplus::Bitmap>(client.right, client.bottom, PixelFormat32bppPARGB);
        frameDirty_ = true;
    }
    if (frameDirty_) {
        // Finish the whole frame before any child copies its pixels. A child can paint before WM_PAINT.
        Gdiplus::Graphics graphics(frame_.get());
        render(graphics, static_cast<float>(client.right), static_cast<float>(client.bottom));
        graphics.Flush(Gdiplus::FlushIntentionSync);
        frameDirty_ = false;
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
    const auto nickname = preview ? std::wstring(L"Steve") : nicknameText();
    if (preview && !originalMode_) player.previewNickname = nickname;
    player.nicknameEmpty = nickname.empty();
    player.nicknameValid = NicknameStore::isValid(nickname);
    player.identitySelection = preview ? (originalMode_ ? 1.0F : 0.0F) : identitySelection_;
    const auto now = wallClock();
    const auto record = playSession_.active() ? playSession_.snapshot(PlaySession::Clock::now(), now) : playtime_;
    player.playing = playSession_.active() || viewState_.operation == LauncherOperation::MinecraftRunning;
    player.lastPlayed = player.playing ? std::wstring(L"Agora") : PlaytimeStore::formatLastPlayed(record.lastPlayed, now);
    player.totalPlayed = record.totalSeconds > 0 || player.playing ? PlaytimeStore::formatDuration(record.totalSeconds) : std::wstring(L"—");
    const bool busy = !viewState_.playEnabled && viewState_.operation != LauncherOperation::MinecraftRunning &&
        viewState_.operation != LauncherOperation::UpdateAvailable;
    if (installation_.visible && installation_.running)
        player.progress = InstallationPanelComponent::overallPercent(installation_, preview ? -1 : installationFill_);
    else if (busy) player.progress = viewState_.progressVisible ? (preview ? static_cast<float>(viewState_.progressPercent) : displayedProgress_) : 0;
    player.phase = animationPhase_;
    player.updateWarning = updateWarning_;
    player.installationVisible = installation_.visible;

    const int serverMods = mods_.manifestKnown ? static_cast<int>(mods_.server.size()) : -1;
    model.chips = {L"Minecraft " + std::wstring(LauncherConfig::MinecraftVersion), L"Forge " + std::wstring(LauncherConfig::ForgeVersion),
        serverMods >= 0 ? std::to_wstring(serverMods) + L" mods" : std::wstring(L"Modpack do grupo")};
    model.reveal = preview ? 1 : portalEntrance_;
    model.energy = preview || !motionEnabled_ ? 0 : std::max({portalEnergy_, portalBurst_, .28F});
    model.phase = animationPhase_;
    model.drift = preview ? 0 : particlePhase_;
    model.toast = toastVisible_ && !installation_.visible;
    model.toastText = toastText_;
    model.serverMods = serverMods;
    model.userMods = modsScanned_ ? static_cast<int>(mods_.user.size()) : -1;
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
    return model;
}

void MainPage::render(Gdiplus::Graphics& g, float width, float height, bool preview)
{
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    g.Clear(Theme::Background);
    const ControlVisuals visuals = [this, preview](int id) { return visual(id, preview); };
    const float s = viewScale_;

    // Content: uniformly scaled and centered, clipped to the area beside the chrome.
    auto saved = g.Save();
    g.SetClip(Gdiplus::RectF(SidebarWidth * s, TitleBarHeight * s, width, height), Gdiplus::CombineModeIntersect);
    g.TranslateTransform(offsetX_, offsetY_);
    g.ScaleTransform(s, s);
    switch (page_) {
    case LauncherPage::Home:
        home_.draw(g, homeModel(preview), hero_, visuals, !installation_.visible);
        if (installation_.visible)
            installationPanel_.draw(g, installation_, animationPhase_, visuals, preview ? -1 : installationFill_,
                preview ? std::array<float, 6>{} : completionPulse_);
        break;
    case LauncherPage::News:
        newsView_.draw(g, NewsModel{&viewState_.news, newsFirst_, newsLoading_, newsError_}, visuals);
        break;
    case LauncherPage::Mods:
        modsView_.draw(g, modsModel(), visuals);
        break;
    case LauncherPage::Settings:
        settingsPage_.draw(g, preview);
        break;
    case LauncherPage::Help:
        helpView_.draw(g, HelpModel{helpMessage_, helpMessageError_, canRepair()}, visuals);
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
    chrome_.draw(g, width / s, height / s, page_, window_ && IsZoomed(window_), windowActive_ || preview, visuals);
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
    frameDirty_ = true;
    if (!animationFrame) syncControls();

    bool unsettled = std::abs(displayedProgress_ - static_cast<float>(viewState_.progressPercent)) > 0.1F || pageFade_ < 1;
    for (const auto& control : controls_) unsettled |= std::abs(control.hover - (control.hovered ? 1.0F : 0.0F)) > 0.005F;
    const float selection = originalMode_ ? 1.0F : 0.0F;
    unsettled |= std::abs(identitySelection_ - selection) > .005F;
    const bool transferVisible = installation_.visible && installation_.running && !installation_.cancelling &&
        transfer.activity == InstallationActivity::Downloading && transfer.percent.has_value();
    unsettled |= transferVisible && std::abs(installationFill_ - fillTarget) > .1F;
    for (const float pulse : completionPulse_) unsettled |= pulse > 0;
    if (!motionEnabled_) {
        for (auto& control : controls_) control.hover = control.hovered ? 1.0F : 0.0F;
        displayedProgress_ = static_cast<float>(viewState_.progressPercent);
        identitySelection_ = selection;
        installationFill_ = fillTarget;
        portalEntrance_ = 1; portalEnergy_ = 0; portalBurst_ = 0; pageFade_ = 1;
        completionPulse_ = {};
        unsettled = false;
    }
    const bool home = page_ == LauncherPage::Home;
    const bool busy = !viewState_.playEnabled && viewState_.operation != LauncherOperation::MinecraftRunning &&
        viewState_.operation != LauncherOperation::UpdateAvailable;
    const bool indeterminate = home && (installation_.visible ? installation_.running && !installation_.cancelling :
        busy && !viewState_.progressVisible);
    const bool hero = home && !installation_.visible;
    const bool ambient = ambientMotion_ && hero && GetForegroundWindow() == window_;
    const bool running = motionEnabled_ && IsWindowVisible(window_) && !IsIconic(window_) && !closing_ &&
        (unsettled || indeterminate || (hero && (ambient || portalEntrance_ < 1 || portalHovered_ || portalEnergy_ > .005F || portalBurst_ > 0)));
    if (running && !animationTimer_) {
        previousFrame_ = std::chrono::steady_clock::now();
        animationTimer_ = SetTimer(window_, AnimationTimerId, USER_TIMER_MINIMUM, nullptr) != 0;
    } else if (!running && animationTimer_) {
        KillTimer(window_, AnimationTimerId);
        animationTimer_ = false;
    }
    // Hover/focus changes the canvas and owner-drawn buttons, not the EDIT contents or caret.
    RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_NOCHILDREN);
    for (const auto& control : controls_)
        if (control.window && (GetWindowLongPtrW(control.window, GWL_STYLE) & WS_VISIBLE) &&
            control.id != NicknameInputId) InvalidateRect(control.window, nullptr, FALSE);
    if (page_ == LauncherPage::Settings) settingsPage_.invalidateButtons();
}

void MainPage::animate()
{
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::clamp(std::chrono::duration<float, std::milli>(now - previousFrame_).count(), 0.0F, 100.0F);
    previousFrame_ = now;
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
    pageFade_ = std::min(1.0F, pageFade_ + dt / 200);
    animationPhase_ = std::fmod(animationPhase_ + dt / 1600, 1.0F);
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
    return id == MinimizeButtonId || id == MaximizeButtonId || id == CloseButtonId || inRange(id, NavHomeId, 5);
}

float MainPage::chromeWidth() const { return viewScale_ > 0 ? clientWidth_ / viewScale_ : static_cast<float>(WindowWidth); }
float MainPage::chromeHeight() const { return viewScale_ > 0 ? clientHeight_ / viewScale_ : static_cast<float>(WindowHeight); }

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
    case NavNewsId: return Layout::navButton(1);
    case NavModsId: return Layout::navButton(2);
    case NavSettingsId: return Layout::navButton(3);
    case NavHelpId: return Layout::sidebarHelp(chromeHeight());
    case NicknameInputId: return Layout::NicknameEdit;
    case EditNicknameButtonId: return Layout::EditNickname;
    case OfflineButtonId: return Layout::OfflineMode;
    case OriginalButtonId: return Layout::OriginalMode;
    case UpdateDownloadButtonId: return Layout::UpdateLink;
    case PlayButtonId: return Layout::Play;
    case PlayMenuButtonId: return Layout::PlayMenu;
    case QuickFolderButtonId: return Layout::quickAction(0);
    case QuickRepairButtonId: return Layout::quickAction(1);
    case QuickLogsButtonId: return Layout::quickAction(2);
    case ToastNewsButtonId: return Layout::ToastButton;
    case NewsAllButtonId: return {Layout::NewsAll.X + 20, Layout::NewsAll.Y, Layout::NewsAll.Width - 20, Layout::NewsAll.Height};
    case InstallationLogButtonId: return Layout::InstallationLog;
    case CancelInstallationButtonId: return Layout::InstallationCancel;
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
    return {};
}

bool MainPage::controlVisible(int id) const
{
    if (chromeControl(id)) return true;
    const bool home = page_ == LauncherPage::Home;
    const bool leftColumn = home && !installation_.visible;
    switch (id) {
    case NicknameInputId:
    case EditNicknameButtonId:
        return home && !originalMode_;
    case OfflineButtonId:
    case OriginalButtonId:
    case PlayButtonId:
    case PlayMenuButtonId:
    case QuickFolderButtonId:
    case QuickRepairButtonId:
    case QuickLogsButtonId:
        return home;
    case UpdateDownloadButtonId:
        return home && !updateWarning_.empty() && viewState_.playEnabled;
    case ToastNewsButtonId:
        return leftColumn && toastVisible_;
    case NewsAllButtonId:
        return leftColumn;
    case InstallationLogButtonId:
        return home && installation_.visible;
    case CancelInstallationButtonId:
        return home && installation_.visible && installation_.running;
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
    default:
        return true;
    }
}

std::wstring MainPage::controlLabel(int id) const
{
    switch (id) {
    case PlayButtonId: return viewState_.primaryActionLabel.empty() ? L"Jogar" : viewState_.primaryActionLabel;
    case OfflineButtonId: return originalMode_ ? L"Nickname" : L"Nickname (selecionado)";
    case OriginalButtonId: return originalMode_ ? L"Conta original (selecionado)" : L"Conta original";
    case MaximizeButtonId: return window_ && IsZoomed(window_) ? L"Restaurar" : L"Maximizar";
    case CancelInstallationButtonId: return installation_.cancelling ? L"Cancelando instalação" : L"Cancelar instalação";
    default: break;
    }
    if (inRange(id, NavHomeId, 5)) {
        const wchar_t* names[] = {L"Início", L"Notícias", L"Mods", L"Configurações", L"Ajuda"};
        const LauncherPage pages[] = {LauncherPage::Home, LauncherPage::News, LauncherPage::Mods, LauncherPage::Settings, LauncherPage::Help};
        const auto index = static_cast<std::size_t>(id - NavHomeId);
        return std::wstring(names[index]) + (page_ == pages[index] ? L" (página atual)" : L"");
    }
    if (inRange(id, NewsCardFirstId, NewsCardCount)) {
        const auto index = static_cast<std::size_t>(id - NewsCardFirstId);
        return index < viewState_.news.size() ? viewState_.news[index].title : std::wstring{};
    }
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
    for (const auto& control : controls_) {
        if (!control.window) continue;
        auto bounds = controlBounds(control.id);
        const float ox = control.chrome ? 0 : offsetX_, oy = control.chrome ? 0 : offsetY_;
        if (control.id == NicknameInputId) {
            // The single-line EDIT draws its text at the top; size it to one line of the display face.
            const float line = 30;
            bounds = {bounds.X, bounds.Y + (bounds.Height - line) / 2, bounds.Width, line};
        }
        const int x = static_cast<int>(std::floor(ox + bounds.X * s)), y = static_cast<int>(std::floor(oy + bounds.Y * s));
        const int right = static_cast<int>(std::ceil(ox + bounds.GetRight() * s));
        const int bottom = static_cast<int>(std::ceil(oy + bounds.GetBottom() * s));
        SetWindowPos(control.window, nullptr, x, y, right - x, bottom - y, SWP_NOZORDER | SWP_NOACTIVATE);
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
    settingsPage_.layout(viewScale_, offsetX_, offsetY_);
}

void MainPage::syncControls()
{
    for (auto& control : controls_) {
        if (!control.window) continue;
        const bool visible = controlVisible(control.id);
        if (((GetWindowLongPtrW(control.window, GWL_STYLE) & WS_VISIBLE) != 0) != visible)
            ShowWindow(control.window, visible ? SW_SHOWNA : SW_HIDE);
        const bool enabled = controlEnabled(control.id);
        if ((IsWindowEnabled(control.window) != FALSE) != enabled) EnableWindow(control.window, enabled);
        if (!enabled || !visible) control.hovered = false;
        if (control.id == NicknameInputId) continue;
        const auto label = controlLabel(control.id);
        std::array<wchar_t, 256> current{};
        GetWindowTextW(control.window, current.data(), static_cast<int>(current.size()));
        if (label != current.data()) SetWindowTextW(control.window, label.c_str());
    }
    if (page_ == LauncherPage::Settings) settingsPage_.setLocked(!viewState_.playEnabled || gameRunning());
    // Never leave keyboard focus on a control that just disappeared.
    const HWND focus = GetFocus();
    if (focus && focus != window_ && IsChild(window_, focus) && !(GetWindowLongPtrW(focus, GWL_STYLE) & WS_VISIBLE)) {
        const int navigation[] = {NavHomeId, NavNewsId, NavModsId, NavSettingsId, NavHelpId};
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
    if (message == WM_MOUSEMOVE && entry && !entry->hovered && IsWindowEnabled(control)) {
        entry->hovered = true;
        TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE, control, 0};
        TrackMouseEvent(&track);
        page->redraw();
    } else if (message == WM_MOUSELEAVE && entry) {
        entry->hovered = false;
        page->redraw();
    } else if (message == WM_SETCURSOR && IsWindowEnabled(control)) {
        const bool caption = id == MinimizeButtonId || id == MaximizeButtonId || id == CloseButtonId;
        SetCursor(LoadCursor(nullptr, caption ? IDC_ARROW : IDC_HAND));
        return TRUE;
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(control, controlProcedure, subclass);
    }
    const auto result = DefSubclassProc(control, message, wParam, lParam);
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
        message == WM_KEYDOWN || message == WM_KEYUP) page->redraw();
    return result;
}

bool MainPage::handleKeyboard(MSG& message)
{
    if (message.message == WM_KEYDOWN) {
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
    if (page_ == LauncherPage::Settings) settingsPage_.show(false);
    page_ = page;
    if (page == LauncherPage::Settings) {
        settingsPage_.open(settings_, settingsWarning_);
        settingsPage_.setLocked(!viewState_.playEnabled || gameRunning());
        settingsPage_.show(true);
    }
    if (page == LauncherPage::Mods) { refreshMods(); modsMessage_.clear(); modsMessageError_ = false; }
    if (page == LauncherPage::News) toastVisible_ = false;
    if (page == LauncherPage::Help) helpMessage_.clear();
    portalHovered_ = false;
    modsHoverRow_ = modsHoverAction_ = -1;
    pageFade_ = motionEnabled_ ? 0.0F : 1.0F;
    redraw();
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
    const auto logs = gameDirectory() / L"logs";
    switch (id) {
    case MinimizeButtonId: PostMessageW(window_, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
    case MaximizeButtonId: PostMessageW(window_, WM_SYSCOMMAND, IsZoomed(window_) ? SC_RESTORE : SC_MAXIMIZE, 0); break;
    case CloseButtonId: PostMessageW(window_, WM_CLOSE, 0, 0); break;
    case NavHomeId: showPage(LauncherPage::Home); break;
    case NavNewsId: showPage(LauncherPage::News); break;
    case NavModsId: showPage(LauncherPage::Mods); break;
    case NavSettingsId: showPage(LauncherPage::Settings); break;
    case NavHelpId: showPage(LauncherPage::Help); break;
    case PlayButtonId: handlePrimaryAction(); break;
    case PlayMenuButtonId: showPlayMenu(); break;
    case OfflineButtonId:
        if (viewState_.playEnabled) { originalMode_ = false; redraw(); }
        break;
    case OriginalButtonId:
        if (viewState_.playEnabled) { originalMode_ = true; redraw(); }
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
    case ToastNewsButtonId:
    case NewsAllButtonId:
        newsFirst_ = 0;
        showPage(LauncherPage::News);
        break;
    case CancelInstallationButtonId: cancelInstallation(); break;
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
    if (page_ == LauncherPage::News) {
        const auto count = viewState_.news.size();
        const auto last = static_cast<int>(count > NewsView::Visible ? count - NewsView::Visible : 0);
        const int first = std::clamp(static_cast<int>(newsFirst_) - notches, 0, last);
        if (static_cast<std::size_t>(first) != newsFirst_) { newsFirst_ = static_cast<std::size_t>(first); redraw(); }
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

void MainPage::checkVersionToast()
{
    const auto directory = LauncherSettingsStore::dataDirectory();
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
    if (previous == current) return;
    // Existing players (a saved nickname) get the toast; a first install does not.
    if (existed || std::filesystem::exists(NicknameStore::filePath(directory), ignored)) {
        toastVisible_ = true;
        toastText_ = L"Launcher atualizado";
    }
    std::filesystem::create_directories(directory, ignored);
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output << current << "\n";
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
        beginPlay();
        return;
    }
    pendingNickname_ = nicknameText();
    pendingIdentity_ = MinecraftIdentity{pendingNickname_};
    if (!NicknameStore::isValid(pendingNickname_)) {
        viewState_.operation = LauncherOperation::InvalidNickname;
        viewState_.headline = L"Confira seu nickname";
        viewState_.operationMessage = L"Use de 3 a 16 letras, números ou underline antes de jogar.";
        viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
        redraw();
        SetFocus(nicknameInput_);
        return;
    }
    if (!NicknameStore::save(LauncherSettingsStore::dataDirectory(), pendingNickname_)) {
        MessageBox(window_, L"Não foi possível salvar o nickname. O jogo ainda pode iniciar, "
                   L"mas talvez você precise digitá-lo novamente na próxima abertura.", L"Nickname", MB_OK | MB_ICONWARNING);
    }
    beginPlay();
}

void MainPage::startRepair()
{
    if (!canRepair()) return;
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
    installation_.begin();
    installationMailbox_.begin(installation_.attempt);
    viewState_.operation = LauncherOperation::PreparingMinecraft;
    viewState_.headline = repairOnly ? L"Reparando a instalação" : L"Preparando seu mundo";
    viewState_.operationMessage = L"Conferindo Java e arquivos do grupo...";
    viewState_.primaryActionLabel = repairOnly ? L"REPARANDO" : L"PREPARANDO";
    viewState_.playEnabled = false;
    viewState_.progressVisible = true;
    viewState_.progressPercent = 0;
    showPage(LauncherPage::Home);
    SetTimer(window_, InstallationProgressTimerId, 200, nullptr);
    redraw();
    const auto attempt = installation_.attempt;
    installationThread_ = std::jthread([this, attempt](std::stop_token stop) {
        InstallationCoordinator coordinator(*fileServerService_, *javaRuntimeService_, *gameInstallationService_,
            *minecraftLauncherService_, settings_);
        auto result = coordinator.run(stop, [this, attempt](const InstallationProgress& value) {
            if (installationMailbox_.publish(attempt, value, InstallationTransferMeter::Clock::now()))
                PostMessageW(window_, InstallationProgressMessage, 0, 0);
        });
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
    viewState_.operation = LauncherOperation::ReadyToPlay;
    viewState_.headline = L"Minecraft encerrado";
    viewState_.operationMessage = session > 0 ? L"Sessão de " + PlaytimeStore::formatDuration(session) +
        L". Você pode jogar de novo quando quiser." : std::wstring(L"Você pode iniciar uma nova sessão quando quiser.");
    viewState_.primaryActionLabel = L"JOGAR";
    viewState_.playEnabled = true;
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
    if (installation_.visible) return;
    if (viewState_.statusCards.size() < 3) return;
    const auto installation = minecraftLauncherService_->validateInstallation();
    updateInstallationState(installation);

    auto& serverCard = viewState_.statusCards[2];
    if (response.success) {
        fileServerManifest_ = response.content;
        refreshMods();
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
        serverCard.value = L"Indisponível";
        serverCard.detail = response.errorMessage.empty() ? L"Não foi possível obter o manifesto" : response.errorMessage;
        serverCard.tone = StatusTone::Error;
        refreshBeforePlay_ = false;
        viewState_.operation = LauncherOperation::ServerUnavailable;
        viewState_.headline = L"Servidor indisponível";
        viewState_.operationMessage = L"Não foi possível atualizar a lista de mods.";
        viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
        viewState_.playEnabled = true;
    }
    redraw();
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
        } else {
            const bool cancelled = result.outcome == InstallationOutcome::Cancelled;
            viewState_.operation = cancelled ? LauncherOperation::NeedsInstallation : LauncherOperation::LaunchError;
            viewState_.headline = cancelled ? L"Instalação cancelada" : L"Falha na preparação";
            viewState_.operationMessage = cancelled ? L"Os arquivos completos serão reaproveitados." : result.message;
            viewState_.primaryActionLabel = cancelled ? L"CONTINUAR" : L"TENTAR NOVAMENTE";
            viewState_.playEnabled = true;
            updateInstallationState(minecraftLauncherService_->validateInstallation());
            if (const auto* play = findControl(PlayButtonId); play && play->window && page_ == LauncherPage::Home) SetFocus(play->window);
        }
    }
    redraw();
}

void MainPage::launchPreparedMinecraft(const InstallationResult& result)
{
    updateInstallationState({true, L"Instalação pronta."});
    auto& modsCard = viewState_.statusCards[1];
    modsCard.value = L"Sincronizados";
    modsCard.detail = std::to_wstring(result.synchronizedFiles) + L" arquivos conferidos";
    modsCard.tone = StatusTone::Success;
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
    viewState_.operationMessage = L"Forge " + std::wstring(LauncherConfig::ForgeVersion) + L" iniciado para " + pendingNickname_ +
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
    playSession_.start(playtime_, PlaySession::Clock::now(), wallClock());
    flushPlaytime();
    SetTimer(window_, PlaytimeTimerId, 60 * 1000, nullptr);
    const int size = GetSystemMetrics(SM_CXSMICON);
    const auto icon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(101), IMAGE_ICON, size, size, LR_SHARED));
    trayIcon_.show(window_, TrayIconMessage, icon ? icon : LoadIcon(nullptr, IDI_APPLICATION), L"Eternal Launcher · Jogando");
    updateTrayTooltip();
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
        playtime_ = {4 * 3600 + 12 * 60, 3600, wallClock() - 26 * 3600};
        originalMode_ = false; updatingLauncher_ = false; updateWarning_.clear();
        installation_ = {};
        toastVisible_ = false;
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
    toastVisible_ = true; toastText_ = L"Launcher atualizado";
    if (!save(L"toast.png", 1)) return false;
    ready(); originalMode_ = true;
    if (!save(L"original.png", 1)) return false;
    ready();
    viewState_.operation = LauncherOperation::ServerUnavailable;
    viewState_.headline = L"Servidor indisponível";
    viewState_.operationMessage = L"Não foi possível consultar o manifesto. Verifique sua conexão e tente novamente.";
    viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
    viewState_.statusCards[2] = {L"SERVIDOR", L"Indisponível", L"Falha ao consultar o manifesto", StatusTone::Error};
    if (!save(L"error.png", 1)) return false;
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
    ready(); page_ = LauncherPage::News;
    if (!save(L"news.png", 1)) return false;
    viewState_.news.clear();
    if (!save(L"news-empty.png", 1)) return false;
    ready(); page_ = LauncherPage::Mods;
    if (!save(L"mods.png", 1)) return false;
    mods_.user.clear();
    if (!save(L"mods-empty.png", 1)) return false;
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
    viewScale_ = 1;
    return true;
}
