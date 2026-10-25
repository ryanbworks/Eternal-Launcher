#include "pages/MainPage.h"

#include "config/LauncherConfig.h"
#include "config/NicknameStore.h"
#include "constants/LauncherConstants.h"
#include "mocks/MockLauncherData.h"
#include "services/HttpFileService.h"
#include "services/GameInstallationService.h"
#include "services/JavaRuntimeService.h"
#include "services/MinecraftLauncherService.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <cmath>

#include <algorithm>
#include <array>
#include <string>

namespace
{
    const Gdiplus::RectF HeroBounds(36, 112, 744, 360);
    const Gdiplus::RectF StatusBounds(36, 496, 744, 128);
    const Gdiplus::RectF NewsBounds(36, 640, 744, 64);
    const Gdiplus::RectF ProfileBounds(832, 136, 288, 232);
    const Gdiplus::RectF PlayBounds(832, 393, 288, 182);
    const Gdiplus::RectF ProgressBounds(832, 593, 288, 112);
    constexpr std::array<int, 4> ActionIds = {
        LauncherConstants::OfflineButtonId, LauncherConstants::OriginalButtonId,
        LauncherConstants::PlayButtonId, LauncherConstants::SettingsButtonId};

}

MainPage::MainPage(HINSTANCE instance)
    : instance_(instance),
      backgroundBrush_(CreateSolidBrush(RGB(13, 14, 20))),
      inputBrush_(CreateSolidBrush(RGB(30, 31, 44))),
      viewState_(MockLauncherData::createInitialState()),
      settings_(LauncherSettingsStore::load(LauncherSettingsStore::dataDirectory(), &settingsWarning_)),
      fileServerService_(std::make_unique<HttpFileService>(
          std::wstring(LauncherConfig::FileServerBaseUrl))),
      javaRuntimeService_(std::make_unique<JavaRuntimeService>()),
      gameInstallationService_(std::make_unique<GameInstallationService>(settings_)),
      minecraftLauncherService_(std::make_unique<MinecraftLauncherService>(settings_))
{
}

MainPage::~MainPage()
{
    installationThread_.request_stop();
    fileServerThread_.request_stop();
    microsoftAuthThread_.request_stop();
    if(installationThread_.joinable()) installationThread_.join();
    if(fileServerThread_.joinable()) fileServerThread_.join();
    if(microsoftAuthThread_.joinable()) microsoftAuthThread_.join();
    launcherUpdateThread_.request_stop();
    if (launcherUpdateThread_.joinable()) launcherUpdateThread_.join();
    if (inputFont_ != nullptr)
    {
        DeleteObject(inputFont_);
    }
    if (inputBrush_ != nullptr)
    {
        DeleteObject(inputBrush_);
    }
    if (backgroundBrush_ != nullptr)
    {
        DeleteObject(backgroundBrush_);
    }
}

bool MainPage::create(int showCommand, bool checkLauncherUpdate, std::wstring updateWarning)
{
    updateWarning_ = std::move(updateWarning);
    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = MainPage::windowProcedure;
    windowClass.lpszClassName = LauncherConstants::WindowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = backgroundBrush_;
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.hIcon = LoadIcon(instance_, MAKEINTRESOURCE(101));
    windowClass.hIconSm = windowClass.hIcon;

    if (RegisterClassEx(&windowClass) == 0)
    {
        return false;
    }

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
        static_cast<float>(workArea.right - workArea.left - 48) / LauncherConstants::WindowWidth,
        static_cast<float>(workArea.bottom - workArea.top - 96) / LauncherConstants::WindowHeight});
    RECT desiredRect{0, 0, static_cast<LONG>(LauncherConstants::WindowWidth * scale),
                           static_cast<LONG>(LauncherConstants::WindowHeight * scale)};
    AdjustWindowRectExForDpi(&desiredRect, windowStyle, FALSE, extendedStyle, dpi);
    const int width = desiredRect.right - desiredRect.left;
    const int height = desiredRect.bottom - desiredRect.top;
    const int x = std::max(workArea.left,
        workArea.left + ((workArea.right - workArea.left) - width) / 2);
    const int y = std::max(workArea.top,
        workArea.top + ((workArea.bottom - workArea.top) - height) / 2);

    window_ = CreateWindowEx(
        extendedStyle,
        LauncherConstants::WindowClassName,
        LauncherConstants::WindowTitle,
        windowStyle,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        instance_,
        this);

    if (window_ == nullptr)
    {
        return false;
    }

    enableDarkTitleBar();
    updateMotionPreference();
    createActionControls();
    createNicknameInput();
    if (!settingsPage_.create(window_, instance_)) { DestroyWindow(window_); return false; }
    layoutControls();
    syncControls();
    portalEntrance_=motionEnabled_ ? 0.0F : 1.0F;
    portalBurst_=motionEnabled_ ? 1.0F : 0.0F;
    ShowWindow(window_, showCommand);
    redraw();
    UpdateWindow(window_);
    if (checkLauncherUpdate) checkLauncherUpdateAsync();
    else checkFileServerAsync();
    return true;
}

LRESULT CALLBACK MainPage::windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainPage* page = nullptr;

    if (message == WM_NCCREATE)
    {
        const auto createData = reinterpret_cast<CREATESTRUCT*>(lParam);
        page = static_cast<MainPage*>(createData->lpCreateParams);
        page->window_ = window;
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(page));
    }
    else
    {
        page = reinterpret_cast<MainPage*>(GetWindowLongPtr(window, GWLP_USERDATA));
    }

    return page != nullptr
        ? page->handleMessage(message, wParam, lParam)
        : DefWindowProc(window, message, wParam, lParam);
}

LRESULT MainPage::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        paint();
        return 0;
    case WM_PRINTCLIENT:
        if(prepareFrame()) {
            Gdiplus::Graphics output(reinterpret_cast<HDC>(wParam));
            output.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            const int width=static_cast<int>(frame_->GetWidth()),height=static_cast<int>(frame_->GetHeight());
            output.DrawImage(frame_.get(),Gdiplus::Rect(0,0,width,height),0,0,width,height,Gdiplus::UnitPixel);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case LauncherConstants::InterfaceRedrawMessage:
        redraw();
        return 0;
    case WM_SIZE:
        leftTransition_.reset();transitionTime_=1;
        pageTravel_=1;
        if (wParam != SIZE_MINIMIZED) layoutControls();
        redraw();
        return 0;
    case WM_SHOWWINDOW:
        if(!wParam) portalHovered_=false;
        // Visibility is applied after this message; defer scheduling until that state is observable.
        PostMessage(window_,LauncherConstants::InterfaceRedrawMessage,0,0);
        return 0;
    case WM_ACTIVATEAPP:
        PostMessage(window_,LauncherConstants::InterfaceRedrawMessage,0,0);return 0;
    case WM_MOUSEMOVE:
    {
        const float x=(static_cast<float>(GET_X_LPARAM(lParam))-offsetX_)/viewScale_-pageOffset();
        const float y=(static_cast<float>(GET_Y_LPARAM(lParam))-offsetY_)/viewScale_;
        const bool hovered=!settingsVisible_ && !installation_.visible && HeroBounds.Contains(x,y);
        if(hovered!=portalHovered_) {
            portalHovered_=hovered;
            TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window_,0};TrackMouseEvent(&track);
            redraw();
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        portalHovered_=false;redraw();return 0;
    case WM_DPICHANGED:
    {
        const auto rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window_, nullptr, rect->left, rect->top,
                     rect->right - rect->left, rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO:
    {
        auto limits = reinterpret_cast<MINMAXINFO*>(lParam);
        limits->ptMinTrackSize = {900, 590};
        return 0;
    }
    case WM_SETTINGCHANGE:
        updateMotionPreference();
        redraw();
        return 0;
    case WM_COMMAND:
        if (settingsVisible_ && SettingsPage::containsId(LOWORD(wParam))) {
            const int id = LOWORD(wParam);
            if (HIWORD(wParam) == BN_CLICKED && (id == SettingsPage::Back || id == SettingsPage::Cancel)) hideSettings();
            else if (HIWORD(wParam) == BN_CLICKED && id == SettingsPage::Save) saveSettings();
            else settingsPage_.handle(id, HIWORD(wParam));
            return 0;
        }
        if (HIWORD(wParam) == BN_CLICKED) {
            if (LOWORD(wParam) == LauncherConstants::UpdateDownloadButtonId) {
                ShellExecuteW(window_, L"open", std::wstring(LauncherConfig::LauncherDownloadUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }
            switch (LOWORD(wParam)) {
            case LauncherConstants::CancelInstallationButtonId: cancelInstallation(); break;
            case LauncherConstants::InstallationLogButtonId: openInstallationLog(); break;
            case LauncherConstants::PlayButtonId: handlePrimaryAction(); break;
            case LauncherConstants::OfflineButtonId:
                if (viewState_.playEnabled) { originalMode_ = false; ShowWindow(nicknameInput_, SW_SHOW); redraw(); }
                break;
            case LauncherConstants::OriginalButtonId:
                if (viewState_.playEnabled) { originalMode_ = true; ShowWindow(nicknameInput_, SW_HIDE); redraw(); }
                break;
            case LauncherConstants::SettingsButtonId: showSettings(); break;
            default: break;
            }
        } else if (LOWORD(wParam) == LauncherConstants::NicknameInputId &&
                   (HIWORD(wParam) == EN_SETFOCUS || HIWORD(wParam) == EN_KILLFOCUS)) redraw();
        return 0;
    case WM_HSCROLL:
        if (settingsVisible_) settingsPage_.handleMemoryScroll(reinterpret_cast<HWND>(lParam));
        return 0;
    case WM_DRAWITEM:
    {
        const auto item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item->CtlType != ODT_BUTTON) return DefWindowProc(window_, message, wParam, lParam);
        if (!prepareFrame()) return TRUE;
        RECT button{}; GetWindowRect(item->hwndItem, &button);
        MapWindowPoints(nullptr, window_, reinterpret_cast<POINT*>(&button), 2);
        Gdiplus::Graphics output(item->hDC);
        output.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        output.DrawImage(frame_.get(), Gdiplus::Rect(0,0,button.right-button.left,button.bottom-button.top),
            button.left,button.top,button.right-button.left,button.bottom-button.top,Gdiplus::UnitPixel);
        return TRUE;
    }
    case WM_TIMER:
        if (wParam == LauncherConstants::GameProcessTimerId) pollGameProcess();
        else if (wParam == LauncherConstants::AnimationTimerId) animate();
        else if (wParam == LauncherConstants::InstallationProgressTimerId) consumeInstallationProgress();
        return 0;
    case LauncherConstants::InstallationProgressMessage:
        consumeInstallationProgress();
        return 0;
    case LauncherConstants::LauncherUpdateProgressMessage:
    {
        std::unique_ptr<LauncherUpdateProgress> progress(reinterpret_cast<LauncherUpdateProgress*>(lParam));
        if (progress) applyLauncherUpdateProgress(*progress);
        return 0;
    }
    case LauncherConstants::LauncherUpdateResultMessage:
    {
        std::unique_ptr<LauncherUpdateResult> result(reinterpret_cast<LauncherUpdateResult*>(lParam));
        if (!result) return 0;
        if (result->restart) {PostMessageW(window_, WM_CLOSE, 0, 0); return 0;}
        if (result->updateAvailable) {
            updatingLauncher_ = false;
            updateWarning_.clear();
            viewState_.progressVisible = false;
            viewState_.progressPercent = 0;
            viewState_.operation = LauncherOperation::UpdateAvailable;
            viewState_.headline = L"Atualização disponível";
            viewState_.operationMessage = L"Uma nova versão do launcher está pronta para baixar.";
            viewState_.primaryActionLabel = L"AGUARDANDO CONFIRMAÇÃO";
            viewState_.playEnabled = false;
            syncControls();
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
                viewState_.primaryActionLabel = L"JOGAR AGORA";
                syncControls();
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
    case LauncherConstants::FileServerResultMessage:
    {
        // Ownership of the result moves to the UI thread at this point.
        std::unique_ptr<FileServerManifestResponse> response(
            reinterpret_cast<FileServerManifestResponse*>(lParam));
        if (response != nullptr)
        {
            applyFileServerResult(*response);
        }
        return 0;
    }
    case LauncherConstants::MicrosoftDeviceCodeMessage:
    {
        std::unique_ptr<MicrosoftDeviceCode> code(reinterpret_cast<MicrosoftDeviceCode*>(lParam));
        if (code)
        {
            const std::wstring prompt = L"Abra " + code->url +
                L" no navegador e digite o código:\n\n" + code->code;
            MessageBox(window_, prompt.c_str(), L"Entrar com Microsoft", MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }
    case LauncherConstants::MicrosoftAuthResultMessage:
    {
        std::unique_ptr<MicrosoftAuthResult> result(reinterpret_cast<MicrosoftAuthResult*>(lParam));
        if (microsoftAuthThread_.joinable()) microsoftAuthThread_.join();
        if (result && result->success)
        {
            microsoftIdentity_ = result->identity;
            pendingIdentity_ = microsoftIdentity_;
            pendingNickname_ = microsoftIdentity_.name;
            redraw();
            beginPlay();
        }
        else
        {
            viewState_.operation = LauncherOperation::AuthenticationError;
            viewState_.headline = L"Falha no login Microsoft";
            viewState_.operationMessage = result ? result->message : L"Falha desconhecida.";
            viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
            viewState_.playEnabled = true;
            redraw();
            MessageBox(window_, viewState_.operationMessage.c_str(), L"Login Microsoft",
                       MB_OK | MB_ICONWARNING);
        }
        return 0;
    }
    case WM_CTLCOLORBTN:
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(13, 14, 20));
        return reinterpret_cast<LRESULT>(backgroundBrush_);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    {
        const HDC editContext = reinterpret_cast<HDC>(wParam);
        SetTextColor(editContext, RGB(245, 247, 250));
        SetBkColor(editContext, RGB(30, 31, 44));
        return reinterpret_cast<LRESULT>(inputBrush_);
    }
    case WM_CLOSE:
    {
        if(installation_.running) {
            closing_=true;
            cancelInstallation();
            return 0; // Continue pumping messages until the installation's terminal event.
        }
        installationThread_.request_stop();
        fileServerThread_.request_stop();
        microsoftAuthThread_.request_stop();
        if(installationThread_.joinable()) installationThread_.join();
        if(fileServerThread_.joinable()) fileServerThread_.join();
        if(microsoftAuthThread_.joinable()) microsoftAuthThread_.join();
        launcherUpdateThread_.request_stop();
        if (launcherUpdateThread_.joinable()) launcherUpdateThread_.join();
        // Drain ownership-bearing messages before the HWND disappears.
        MSG pending{};
        while (PeekMessageW(&pending, window_, LauncherConstants::LauncherUpdateProgressMessage,
            LauncherConstants::LauncherUpdateResultMessage, PM_REMOVE)) {
            if (pending.message == LauncherConstants::LauncherUpdateProgressMessage)
                delete reinterpret_cast<LauncherUpdateProgress*>(pending.lParam);
            else delete reinterpret_cast<LauncherUpdateResult*>(pending.lParam);
        }
        while(PeekMessageW(&pending,window_,LauncherConstants::FileServerResultMessage,LauncherConstants::MicrosoftAuthResultMessage,PM_REMOVE)) {
            if(pending.message==LauncherConstants::FileServerResultMessage) delete reinterpret_cast<FileServerManifestResponse*>(pending.lParam);
            else if(pending.message==LauncherConstants::MicrosoftDeviceCodeMessage) delete reinterpret_cast<MicrosoftDeviceCode*>(pending.lParam);
            else if(pending.message==LauncherConstants::MicrosoftAuthResultMessage) delete reinterpret_cast<MicrosoftAuthResult*>(pending.lParam);
        }
        // Also save when the user closes without clicking Play.
        std::array<wchar_t, 32> nickname{};
        GetWindowText(nicknameInput_, nickname.data(), static_cast<int>(nickname.size()));
        if (NicknameStore::isValid(nickname.data()))
        {
            (void)NicknameStore::save(
                LauncherSettingsStore::dataDirectory(), nickname.data());
        }
        DestroyWindow(window_);
        return 0;
    }
    case WM_DESTROY:
        launcherUpdateThread_.request_stop();
        KillTimer(window_, LauncherConstants::GameProcessTimerId);
        KillTimer(window_, LauncherConstants::AnimationTimerId);
        KillTimer(window_, LauncherConstants::InstallationProgressTimerId);
        fileServerThread_.request_stop();
        installationThread_.request_stop();
        microsoftAuthThread_.request_stop();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(window_, message, wParam, lParam);
    }
}

void MainPage::paint()
{
    PAINTSTRUCT ps{};
    const HDC dc = BeginPaint(window_, &ps);
    if (prepareFrame()) {
        Gdiplus::Graphics output(dc);
        output.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        // Explicit pixel bounds prevent GDI+ from scaling the buffer again at 125%/150% DPI.
        const int width=static_cast<int>(frame_->GetWidth()),height=static_cast<int>(frame_->GetHeight());
        output.DrawImage(frame_.get(),Gdiplus::Rect(0,0,width,height),0,0,width,height,Gdiplus::UnitPixel);
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
        render(graphics);
        graphics.Flush(Gdiplus::FlushIntentionSync);
        frameDirty_ = false;
    }
    return true;
}

bool MainPage::renderSettingsTransition(Gdiplus::Graphics& g)
{
    RECT client{};GetClientRect(window_,&client);
    if(client.right<=0 || client.bottom<=0) return false;
    if(!settingsTransitionFrame_) {
        auto cache=std::make_unique<Gdiplus::Bitmap>(client.right,client.bottom,PixelFormat32bppPARGB);
        if(cache->GetLastStatus()!=Gdiplus::Ok) return false;
        Gdiplus::Graphics drawing(cache.get());
        render(drawing,false,true);
        drawing.Flush(Gdiplus::FlushIntentionSync);
        settingsTransitionFrame_=std::move(cache);
    }
    // Reuse the current form at rest. Real native controls still move/paint independently.
    // Integer pixel copies keep ClearType sharp and leave the fixed header untouched.
    const int width=client.right,height=client.bottom;
    const int bodyY=std::clamp(static_cast<int>(std::floor(offsetY_+104*viewScale_)),0,height);
    const int travel=static_cast<int>(std::round(pageOffset()*viewScale_));
    const auto saved=g.Save();
    g.Clear(Theme::Background);
    g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.DrawImage(settingsTransitionFrame_.get(),Gdiplus::Rect(0,0,width,bodyY),0,0,width,bodyY,Gdiplus::UnitPixel);
    g.SetClip(Gdiplus::Rect(0,bodyY,width,height-bodyY),Gdiplus::CombineModeIntersect);
    g.DrawImage(settingsTransitionFrame_.get(),Gdiplus::Rect(travel,0,width,height),0,0,width,height,Gdiplus::UnitPixel);
    g.Restore(saved);
    return true;
}

void MainPage::render(Gdiplus::Graphics& g, bool preview, bool restingSettings)
{
    if(!preview && !restingSettings && settingsVisible_ && pageTravel_<1 && renderSettingsTransition(g)) return;
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    g.Clear(Theme::Background);
    const auto saved = g.Save();
    g.TranslateTransform(offsetX_, offsetY_);
    g.ScaleTransform(viewScale_, viewScale_);
    const auto focus = GetFocus();
    header_.draw(g, static_cast<float>(LauncherConstants::WindowWidth), hover_[3],
                 !preview && focus == actions_[3], !settingsVisible_);
    if(!preview && !restingSettings) g.TranslateTransform(pageOffset(),0);
    if (settingsVisible_) { settingsPage_.draw(g, preview); g.Restore(saved); return; }
    if(installation_.visible) installationPanel_.draw(g,installation_,animationPhase_,
        !preview && focus==cancelInstallationButton_,!preview && focus==installationLogButton_,
        preview ? 0 : installationHover_[0],preview ? 0 : installationHover_[1],
        !preview && cancelInstallationButton_ && (SendMessageW(cancelInstallationButton_,BM_GETSTATE,0,0)&BST_PUSHED),
        !preview && installationLogButton_ && (SendMessageW(installationLogButton_,BM_GETSTATE,0,0)&BST_PUSHED),
        preview ? -1 : installationFill_,preview ? std::array<float,6>{} : completionPulse_);
    else {
        hero_.draw(g, HeroBounds,preview ? 1 : portalEntrance_,
            preview || !motionEnabled_ ? 0 : std::max({portalEnergy_,portalBurst_,.28F}),animationPhase_,particlePhase_);
        statusPanel_.draw(g, StatusBounds, viewState_.statusCards);
        newsPanel_.draw(g, NewsBounds, viewState_.featuredNews);
    }
    DrawingUtils::fillRoundedRectangle(g, {808,112,336,592}, Theme::PanelRadius, Theme::Surface);
    int profileFocus = 0;
    if (!preview) {
        if (focus == actions_[0]) profileFocus = 1;
        if (focus == actions_[1]) profileFocus = 2;
        if (focus == nicknameInput_) profileFocus = 3;
    }
    playerProfile_.draw(g, ProfileBounds, originalMode_, microsoftIdentity_.name,
                       hover_[0], hover_[1], profileFocus, viewState_.playEnabled,preview ? -1 : identitySelection_);
    playPanel_.draw(g, PlayBounds, viewState_, hover_[2], !preview && focus == actions_[2],
                   !preview && (SendMessage(actions_[2], BM_GETSTATE, 0, 0) & BST_PUSHED) != 0,
                   preview ? 0 : playPress_,preview ? 1 : playSweep_,preview ? 0 : playPulse_,
                   playLight_,animationPhase_,!preview && motionEnabled_);
    const bool error = isLauncherError(viewState_.operation);
    const bool needsInstallation = viewState_.operation == LauncherOperation::NeedsInstallation;
    const bool busy = viewState_.progressVisible ||
                      (!viewState_.playEnabled && viewState_.operation != LauncherOperation::MinecraftRunning);
    const bool updateNotice = !busy && !updateWarning_.empty();
    progress_.draw(g, ProgressBounds, viewState_.progressPercent,
                   installation_.visible ? L"Acompanhe as etapas no painel ao lado." : updateNotice ? updateWarning_ : busy ? viewState_.operationMessage : error ? L"Consulte o status acima e tente novamente."
                   : needsInstallation ? L"Java, Minecraft e Forge serão preparados ao jogar."
                   : L"Nenhuma operação de arquivo em andamento.",
                   busy && !installation_.visible, !error && !updateNotice, displayedProgress_, animationPhase_,
                   installation_.visible ? L"Preparação do jogo" : updatingLauncher_ ? L"Atualização do launcher" : updateNotice ? L"Atualização pendente" : L"", updateNotice);
    if (updateNotice && !preview && GetFocus() == updateDownloadButton_)
        DrawingUtils::drawRoundedRectangle(g,{831,681,290,24},3,Theme::AccentLight,1);
    if (preview && !originalMode_) {
        const auto input = playerProfile_.inputBounds(ProfileBounds);
        DrawingUtils::drawText(g, L"Steve", {input.X+14,input.Y,input.Width-28,input.Height},15,
            Gdiplus::FontStyleRegular,Theme::TextPrimary,Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
    }
    g.Restore(saved);
    if(!preview && leftTransition_ && transitionTime_<1) {
        // A portal shutter replaces the old scene without superimposing two sets of copy.
        // Native actions and keyboard focus remain outside this cached region.
        const float remaining=1-transitionTime_;
        const float edge=transitionBounds_.X+transitionBounds_.Width*(1-remaining*remaining*remaining);
        const auto overlay=g.Save();
        g.SetClip(Gdiplus::RectF(edge,transitionBounds_.Y,transitionBounds_.GetRight()-edge,transitionBounds_.Height),
            Gdiplus::CombineModeIntersect);
        g.DrawImage(leftTransition_.get(),transitionBounds_,0,0,
            static_cast<float>(leftTransition_->GetWidth()),static_cast<float>(leftTransition_->GetHeight()),
            Gdiplus::UnitPixel);
        g.Restore(overlay);
    }
}

void MainPage::enableDarkTitleBar() const
{
    // Blend the native frame into the dark theme when the Windows version supports it.
    constexpr DWORD useImmersiveDarkMode = 20;
    BOOL enabled = TRUE;
    DwmSetWindowAttribute(window_, useImmersiveDarkMode, &enabled, sizeof(enabled));
}

void MainPage::createNicknameInput()
{
    const auto savedNickname = NicknameStore::load(
        LauncherSettingsStore::dataDirectory());
    nicknameInput_ = CreateWindowEx(
        0,
        L"EDIT",
        savedNickname.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(LauncherConstants::NicknameInputId)),
        instance_,
        nullptr);

    inputFont_ = CreateFont(
        -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(nicknameInput_, WM_SETFONT, reinterpret_cast<WPARAM>(inputFont_), TRUE);
    SendMessage(nicknameInput_, EM_SETCUEBANNER, TRUE,
                reinterpret_cast<LPARAM>(L"Digite seu nickname"));
    SendMessage(nicknameInput_, EM_SETLIMITTEXT, 16, 0);
    SetWindowTheme(nicknameInput_, L"DarkMode_Explorer", nullptr);
    layoutNicknameInput();
}

void MainPage::layoutNicknameInput() const
{
    const auto field = playerProfile_.inputBounds(ProfileBounds);
    SetWindowPos(nicknameInput_, nullptr,
        static_cast<int>(offsetX_ + (field.X+14)*viewScale_),
        static_cast<int>(offsetY_ + (field.Y+12)*viewScale_),
        static_cast<int>((field.Width-28)*viewScale_),
        static_cast<int>((field.Height-20)*viewScale_), SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainPage::handlePrimaryAction()
{
    if (!viewState_.playEnabled)
    {
        return;
    }

    std::wstring settingsError;
    if (!LauncherSettingsStore::validate(settings_, settingsError)) {
        showSettings(); settingsPage_.setMessage(settingsError, true); return;
    }

    if (originalMode_)
    {
        if (!microsoftIdentity_.online())
        {
            startMicrosoftSignIn();
            return;
        }
        pendingIdentity_ = microsoftIdentity_;
        pendingNickname_ = microsoftIdentity_.name;
        beginPlay();
        return;
    }

    std::array<wchar_t, 32> nickname{};
    GetWindowText(nicknameInput_, nickname.data(), static_cast<int>(nickname.size()));
    pendingNickname_ = nickname.data();
    pendingIdentity_ = MinecraftIdentity{pendingNickname_};
    if (!NicknameStore::isValid(pendingNickname_))
    {
        viewState_.operation = LauncherOperation::InvalidNickname;
        viewState_.headline = L"Confira seu nickname";
        viewState_.operationMessage =
            L"Use de 3 a 16 letras, números ou underline antes de instalar.";
        viewState_.primaryActionLabel = L"TENTAR NOVAMENTE";
        redraw();
        SetFocus(nicknameInput_);
        return;
    }

    if (!NicknameStore::save(
            LauncherSettingsStore::dataDirectory(), pendingNickname_))
    {
        MessageBox(window_, L"Não foi possível salvar o nickname. O jogo ainda pode iniciar, "
                   L"mas talvez você precise digitá-lo novamente na próxima abertura.",
                   L"Nickname", MB_OK | MB_ICONWARNING);
    }

    beginPlay();
}

void MainPage::startMicrosoftSignIn()
{
    if (microsoftAuthThread_.joinable()) microsoftAuthThread_.join();
    viewState_.operation = LauncherOperation::SigningIn;
    viewState_.headline = L"Entrando com Microsoft";
    viewState_.operationMessage = L"Aguarde o código de confirmação...";
    viewState_.primaryActionLabel = L"CONECTANDO...";
    viewState_.playEnabled = false;
    redraw();
    microsoftAuthThread_ = std::jthread([this](std::stop_token stopToken)
    {
        MicrosoftAuthService service;
        auto result = service.signIn(stopToken, [this, stopToken](const MicrosoftDeviceCode& code)
        {
            if (stopToken.stop_requested()) return;
            auto* transferred = new MicrosoftDeviceCode(code);
            if (!PostMessage(window_, LauncherConstants::MicrosoftDeviceCodeMessage, 0,
                             reinterpret_cast<LPARAM>(transferred))) delete transferred;
        });
        if (stopToken.stop_requested()) return;
        auto* transferred = new MicrosoftAuthResult(std::move(result));
        if (!PostMessage(window_, LauncherConstants::MicrosoftAuthResultMessage, 0,
                         reinterpret_cast<LPARAM>(transferred))) delete transferred;
    });
}

void MainPage::beginPlay()
{
    if (installation_.running || updatingLauncher_ || minecraftLauncherService_->isGameRunning()) return;
    if (installationThread_.joinable()) installationThread_.join();
    fileServerThread_.request_stop();
    if (fileServerThread_.joinable()) fileServerThread_.join();
    installation_.begin();
    installationMailbox_.begin(installation_.attempt);
    viewState_.operation=LauncherOperation::PreparingMinecraft;
    viewState_.headline=L"Preparando seu mundo";
    viewState_.operationMessage=L"Conferindo Java e arquivos do grupo...";
    viewState_.primaryActionLabel=L"PREPARANDO...";
    viewState_.playEnabled=false;
    viewState_.progressVisible=true;
    viewState_.progressPercent=0;
    layoutControls();
    SetTimer(window_,LauncherConstants::InstallationProgressTimerId,200,nullptr);
    redraw();
    const auto attempt=installation_.attempt;
    installationThread_=std::jthread([this,attempt](std::stop_token stop) {
        InstallationCoordinator coordinator(*fileServerService_,*javaRuntimeService_,*gameInstallationService_,*minecraftLauncherService_,settings_);
        auto result=coordinator.run(stop,[this,attempt](const InstallationProgress& value) {
            if(installationMailbox_.publish(attempt,value,InstallationTransferMeter::Clock::now()))
                PostMessageW(window_,LauncherConstants::InstallationProgressMessage,0,0);
        });
        installationMailbox_.finish(attempt,std::move(result));
        PostMessageW(window_,LauncherConstants::InstallationProgressMessage,0,0);
    });
}
void MainPage::pollGameProcess()
{
    if (minecraftLauncherService_->isGameRunning())
    {
        return;
    }

    KillTimer(window_, LauncherConstants::GameProcessTimerId);
    if (minimizedForGame_) { ShowWindow(window_, SW_RESTORE); minimizedForGame_ = false; }
    viewState_.operation = LauncherOperation::ReadyToPlay;
    viewState_.headline = L"Minecraft encerrado";
    viewState_.operationMessage = L"Você pode iniciar uma nova sessão quando quiser.";
    viewState_.primaryActionLabel = L"JOGAR NOVAMENTE";
    viewState_.playEnabled = true;
    redraw();
}

void MainPage::showSettings()
{
    if(settingsVisible_) return;
    settingsPage_.open(settings_, settingsWarning_);
    settingsVisible_ = true;
    pageTravel_=motionEnabled_ ? 0.0F : 1.0F;
    for (const auto action : actions_) ShowWindow(action, SW_HIDE);
    ShowWindow(nicknameInput_, SW_HIDE);
    settingsPage_.setLocked(!viewState_.playEnabled || minecraftLauncherService_->isGameRunning());
    settingsPage_.show(true);
    redraw();
}

void MainPage::hideSettings()
{
    if(!settingsVisible_) return;
    settingsVisible_ = false;
    pageTravel_=motionEnabled_ ? 0.0F : 1.0F;
    settingsPage_.show(false);
    for (const auto action : actions_) ShowWindow(action, SW_SHOW);
    ShowWindow(nicknameInput_, originalMode_ ? SW_HIDE : SW_SHOW);
    SetFocus(actions_[3]);
    redraw();
}

void MainPage::saveSettings()
{
    if (!viewState_.playEnabled || minecraftLauncherService_->isGameRunning()) return;
    LauncherSettings candidate;
    if (!settingsPage_.collect(candidate)) return;
    std::wstring error;
    if (!LauncherSettingsStore::save(LauncherSettingsStore::dataDirectory(), candidate, error)) {
        settingsPage_.setMessage(error, true); return;
    }
    settings_ = std::move(candidate);
    installation_.visible=false;
    settingsWarning_.clear();
    // No active operation or game can hold these services while the form is editable.
    minecraftLauncherService_ = std::make_unique<MinecraftLauncherService>(settings_);
    gameInstallationService_ = std::make_unique<GameInstallationService>(settings_);
    settingsPage_.open(settings_, {});
    settingsPage_.setMessage(L"Preferências salvas. Serão usadas na próxima sessão.");
    viewState_.operation = LauncherOperation::CheckingUpdates;
    viewState_.headline = L"Conferindo a instância";
    viewState_.operationMessage = L"Verificando a instalação e o servidor de arquivos...";
    viewState_.primaryActionLabel = L"VERIFICANDO...";
    viewState_.playEnabled = false;
    redraw();
    checkFileServerAsync();
}

void MainPage::checkFileServerAsync()
{
    // The HTTP call never runs on the window thread, keeping the
    // nickname field, drawing and other controls responsive.
    if (fileServerThread_.joinable()) fileServerThread_.join();
    fileServerThread_ = std::jthread([this](std::stop_token stopToken)
    {
        auto response = fileServerService_->fetchManifest(stopToken);
        if (stopToken.stop_requested())
        {
            return;
        }

        auto* transferredResponse = new FileServerManifestResponse(std::move(response));
        if (!PostMessage(
                window_, LauncherConstants::FileServerResultMessage, 0,
                reinterpret_cast<LPARAM>(transferredResponse)))
        {
            // If the window was closed during the request, nobody will
            // receive the message, so the result must be freed here.
            delete transferredResponse;
        }
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
            if (!PostMessageW(window_, LauncherConstants::LauncherUpdateProgressMessage, 0, reinterpret_cast<LPARAM>(progress))) delete progress;
        }, transport, confirmed);
        if (stop.stop_requested()) return;
        auto* transferred = new LauncherUpdateResult(std::move(result));
        if (!PostMessageW(window_, LauncherConstants::LauncherUpdateResultMessage, 0, reinterpret_cast<LPARAM>(transferred))) delete transferred;
    });
}

void MainPage::applyLauncherUpdateProgress(const LauncherUpdateProgress& progress)
{
    viewState_.playEnabled = false;
    viewState_.progressVisible = true;
    viewState_.progressPercent = progress.percent;
    switch (progress.phase) {
    case LauncherUpdatePhase::Checking:
        viewState_.operation = LauncherOperation::CheckingLauncherUpdate;
        viewState_.headline = L"Verificando atualização";
        viewState_.operationMessage = L"Consultando a versão mais recente do launcher...";
        viewState_.primaryActionLabel = L"VERIFICANDO...";
        break;
    case LauncherUpdatePhase::Downloading:
        viewState_.operation = LauncherOperation::DownloadingLauncherUpdate;
        viewState_.headline = L"Baixando atualização";
        viewState_.operationMessage = L"O launcher vai reabrir ao concluir. Seus dados serão preservados.";
        viewState_.primaryActionLabel = L"BAIXANDO...";
        break;
    case LauncherUpdatePhase::Applying:
        viewState_.operation = LauncherOperation::ApplyingLauncherUpdate;
        viewState_.headline = L"Aplicando atualização";
        viewState_.operationMessage = L"Verificando o arquivo e preparando a troca...";
        viewState_.primaryActionLabel = L"ATUALIZANDO...";
        break;
    case LauncherUpdatePhase::Restarting:
        viewState_.operation = LauncherOperation::RestartingLauncher;
        viewState_.headline = L"Reabrindo launcher";
        viewState_.operationMessage = L"Tudo pronto. A nova versão vai abrir automaticamente.";
        viewState_.primaryActionLabel = L"REABRINDO...";
        break;
    }
    redraw();
}

void MainPage::applyFileServerResult(const FileServerManifestResponse& response)
{
    if(installation_.visible) return;
    if (viewState_.statusCards.size() < 3)
    {
        return;
    }

    const auto installation = minecraftLauncherService_->validateInstallation();
    updateInstallationState(installation);

    auto& serverCard = viewState_.statusCards[2];
    if (response.success)
    {
        fileServerManifest_ = response.content;
        serverCard.value = L"Disponível";
        serverCard.detail = L"HTTP " + std::to_wstring(response.httpStatus) +
                            L" • " + std::to_wstring(response.elapsedMilliseconds) + L" ms";
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
        if (refreshBeforePlay_)
        {
            refreshBeforePlay_ = false;
            // Only clicking Play starts downloads and Minecraft preparation.
            beginPlay();
            return;
        }

        // Opening the screen only checks the local state and the server. Downloads and
        // sync are reserved for an explicit Play click.
        viewState_.operation = installation.success
            ? LauncherOperation::ReadyToPlay
            : LauncherOperation::NeedsInstallation;
        viewState_.headline = installation.success
            ? L"Pronto para jogar"
            : L"Pronto para instalar";
        viewState_.operationMessage = installation.success
            ? L"Clique em Jogar para conferir os arquivos do grupo e iniciar."
            : L"Clique em Jogar para preparar Java, Minecraft e Forge.";
        viewState_.primaryActionLabel = installation.success
            ? L"JOGAR AGORA"
            : L"INSTALAR E JOGAR";
        viewState_.playEnabled = true;
    }
    else
    {
        serverCard.value = L"Indisponível";
        serverCard.detail = response.errorMessage.empty()
            ? L"Não foi possível obter o manifesto"
            : response.errorMessage;
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
    if(!installation_.running || installation_.cancelling) return;
    installation_.cancelling=true;
    installationThread_.request_stop();
    viewState_.headline=L"Cancelando instalação...";
    viewState_.operationMessage=L"Aguardando as tarefas encerrarem com segurança...";
    viewState_.primaryActionLabel=L"CANCELANDO...";
    redraw();
}

void MainPage::openInstallationLog()
{
    const auto directory=LauncherSettingsStore::effectiveGameDirectory(settings_)/L"logs";
    const auto log=directory/L"installation.log";
    std::error_code ignored;
    const bool hasLog=std::filesystem::is_regular_file(log,ignored);
    const auto target=hasLog ? log : directory;
    const auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(window_,L"open",target.c_str(),nullptr,nullptr,SW_SHOWNORMAL));
    if(result<=32) {
        viewState_.operationMessage=L"Não foi possível abrir o log. Confira as permissões da pasta do jogo.";
        redraw();
    }
}

void MainPage::consumeInstallationProgress()
{
    auto snapshot=installationMailbox_.take();
    if(!snapshot) {
        if(installation_.running && !installation_.cancelling && installation_.progress.activity==InstallationActivity::Downloading) {
            installation_.meter.sample(installation_.progress.receivedBytes,InstallationTransferMeter::Clock::now());
            redraw();
        }
        return;
    }
    if(snapshot->attempt!=installation_.attempt || !installation_.running) return;
    if(installation_.apply(snapshot->attempt,snapshot->progress,InstallationTransferMeter::Clock::now())) {
        installation_.steps=snapshot->steps;
        viewState_.headline=installationStageLabel(snapshot->progress.stage);
        viewState_.operationMessage=snapshot->progress.message;
        viewState_.progressPercent=snapshot->progress.activity==InstallationActivity::Downloading ? snapshot->progress.percent.value_or(0) : 0;
    }
    if(snapshot->result && installation_.finish(snapshot->attempt,*snapshot->result)) {
        // The worker has published its terminal result; joining here only completes cleanup.
        if(installationThread_.joinable()) installationThread_.join();
        KillTimer(window_,LauncherConstants::InstallationProgressTimerId);
        if(closing_) {SendMessageW(window_,WM_CLOSE,0,0);return;}
        viewState_.progressVisible=false;viewState_.progressPercent=0;
        const auto result=*installation_.result;
        if(result.outcome==InstallationOutcome::Success) launchPreparedMinecraft(result);
        else {
            const bool cancelled=result.outcome==InstallationOutcome::Cancelled;
            viewState_.operation=cancelled ? LauncherOperation::NeedsInstallation : LauncherOperation::LaunchError;
            viewState_.headline=cancelled ? L"Instalação cancelada" : L"Falha na preparação";
            viewState_.operationMessage=cancelled ? L"Os arquivos completos serão reaproveitados." : result.message;
            viewState_.primaryActionLabel=cancelled ? L"CONTINUAR INSTALAÇÃO" : L"TENTAR NOVAMENTE";
            viewState_.playEnabled=true;
            updateInstallationState(minecraftLauncherService_->validateInstallation());
            SetFocus(actions_[2]);
        }
    }
    redraw();
}

void MainPage::launchPreparedMinecraft(const InstallationResult& result)
{
    updateInstallationState({true,L"Instalação pronta."});
    auto& modsCard=viewState_.statusCards[1];
    modsCard.value=L"Sincronizados";
    modsCard.detail=std::to_wstring(result.synchronizedFiles)+L" arquivos do manifesto privado";
    modsCard.tone=StatusTone::Success;
    viewState_.operation=LauncherOperation::LaunchingMinecraft;
    viewState_.headline=L"Iniciando Minecraft";
    viewState_.operationMessage=L"Preparação concluída; iniciando Forge 47.4.10...";
    viewState_.primaryActionLabel=L"INICIANDO...";
    viewState_.playEnabled=false;
    redraw();UpdateWindow(window_);
    const auto launched=minecraftLauncherService_->launch(pendingIdentity_);
    if(!launched.success) {
        InstallationCoordinator::recordLaunchFailure(settings_,launched.message);
        installation_.result=InstallationResult{InstallationOutcome::Failed,InstallationFailure::Process,
            L"Não foi possível iniciar o jogo. Tente novamente ou abra o log.",launched.message,0};
        installation_.steps[5]=InstallationStepState::Failed;
        viewState_.operation=LauncherOperation::LaunchError;
        viewState_.headline=L"Não foi possível iniciar";
        viewState_.operationMessage=installation_.result->message;
        viewState_.primaryActionLabel=L"TENTAR NOVAMENTE";
        viewState_.playEnabled=true;
    } else {
        installation_.visible=false;
        viewState_.operation=LauncherOperation::MinecraftRunning;
        viewState_.headline=L"Minecraft em execução";
        viewState_.operationMessage=L"Forge 47.4.10 iniciado para "+pendingNickname_+L".";
        viewState_.primaryActionLabel=L"JOGO EM EXECUÇÃO";
        SetTimer(window_,LauncherConstants::GameProcessTimerId,1000,nullptr);
        if(settings_.minimizeOnLaunch) {ShowWindow(window_,SW_MINIMIZE);minimizedForGame_=true;}
    }
}
void MainPage::updateInstallationState(const MinecraftLaunchResult& installation)
{
    auto& installationCard = viewState_.statusCards[0];
    if (installation.success)
    {
        installationCard.value = L"Pronta";
        installationCard.detail = L"Minecraft 1.20.1 + Forge 47.4.10";
        installationCard.tone = StatusTone::Success;
        return;
    }

    installationCard.value = L"Incompleta";
    switch (installation.issue)
    {
    case MinecraftLaunchIssue::InvalidConfiguration:
        installationCard.detail = L"Confira Java e preferências";
        break;
    case MinecraftLaunchIssue::JavaMissing:
        installationCard.detail = L"Java 17 será baixado automaticamente";
        break;
    case MinecraftLaunchIssue::NativeLibrariesMissing:
        installationCard.detail = L"Bibliotecas nativas ausentes";
        break;
    default:
        installationCard.detail = L"Arquivos do Minecraft/Forge ausentes";
        break;
    }
    installationCard.tone = StatusTone::Warning;
}

void MainPage::createActionControls()
{
    const wchar_t* labels[] = {L"Nickname local", L"Original", L"Jogar", L"Configurações"};
    for (std::size_t i = 0; i < actions_.size(); ++i) {
        actions_[i] = CreateWindowEx(0, L"BUTTON", labels[i],
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0,0,0,0, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ActionIds[i])), instance_, nullptr);
        SetWindowSubclass(actions_[i], controlProcedure, i+1, reinterpret_cast<DWORD_PTR>(this));
    }
    updateDownloadButton_ = CreateWindowExW(0, L"BUTTON", L"Baixar launcher", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(LauncherConstants::UpdateDownloadButtonId)),instance_,nullptr);
    SetWindowSubclass(updateDownloadButton_, controlProcedure, 10, reinterpret_cast<DWORD_PTR>(this));
    cancelInstallationButton_=CreateWindowExW(0,L"BUTTON",L"Cancelar instalação",WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,
        0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(LauncherConstants::CancelInstallationButtonId)),instance_,nullptr);
    installationLogButton_=CreateWindowExW(0,L"BUTTON",L"Abrir log",WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,
        0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(LauncherConstants::InstallationLogButtonId)),instance_,nullptr);
    SetWindowSubclass(cancelInstallationButton_,controlProcedure,11,reinterpret_cast<DWORD_PTR>(this));
    SetWindowSubclass(installationLogButton_,controlProcedure,12,reinterpret_cast<DWORD_PTR>(this));
}

void MainPage::layoutControls()
{
    settingsTransitionFrame_.reset();
    RECT client{}; GetClientRect(window_, &client);
    if (client.right <= 0 || client.bottom <= 0) return;
    viewScale_ = std::min(static_cast<float>(client.right) / LauncherConstants::WindowWidth,
                         static_cast<float>(client.bottom) / LauncherConstants::WindowHeight);
    offsetX_ = (client.right - LauncherConstants::WindowWidth * viewScale_) / 2;
    offsetY_ = (client.bottom - LauncherConstants::WindowHeight * viewScale_) / 2;
    const std::array<Gdiplus::RectF,4> bounds = {
        playerProfile_.offlineTabBounds(ProfileBounds), playerProfile_.originalTabBounds(ProfileBounds),
        playPanel_.buttonBounds(PlayBounds), header_.settingsBounds(static_cast<float>(LauncherConstants::WindowWidth))};
    for (std::size_t i=0; i<actions_.size(); ++i) {
        if (!actions_[i]) continue;
        const auto& b=bounds[i];
        const int x=static_cast<int>(std::floor(offsetX_+b.X*viewScale_));
        const int y=static_cast<int>(std::floor(offsetY_+b.Y*viewScale_));
        const int right=static_cast<int>(std::ceil(offsetX_+b.GetRight()*viewScale_));
        const int bottom=static_cast<int>(std::ceil(offsetY_+b.GetBottom()*viewScale_));
        SetWindowPos(actions_[i],nullptr,x,y,right-x,bottom-y,SWP_NOZORDER | SWP_NOACTIVATE);
    }
    settingsPage_.layout(viewScale_, offsetX_, offsetY_);
    for(const auto& item:{std::pair{cancelInstallationButton_,InstallationPanelComponent::cancelBounds()},
        std::pair{installationLogButton_,InstallationPanelComponent::logBounds()}}) {
        if(!item.first) continue;
        const auto& controlBounds=item.second;
        SetWindowPos(item.first,nullptr,static_cast<int>(offsetX_+controlBounds.X*viewScale_),static_cast<int>(offsetY_+controlBounds.Y*viewScale_),
            static_cast<int>(controlBounds.Width*viewScale_),static_cast<int>(controlBounds.Height*viewScale_),SWP_NOZORDER|SWP_NOACTIVATE);
    }
    if(installationLogButton_ && cancelInstallationButton_) {
        SetWindowPos(installationLogButton_,actions_[2],0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        SetWindowPos(cancelInstallationButton_,installationLogButton_,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
    if(updateDownloadButton_) SetWindowPos(updateDownloadButton_,nullptr,
        static_cast<int>(offsetX_+832*viewScale_),static_cast<int>(offsetY_+681*viewScale_),
        static_cast<int>(288*viewScale_),static_cast<int>(24*viewScale_),SWP_NOZORDER | SWP_NOACTIVATE);
    if (nicknameInput_) {
        // Child Z order also defines Win32 dialog tab order.
        SetWindowPos(nicknameInput_, actions_[1], 0,0,0,0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        const HFONT font=CreateFont(-static_cast<int>(std::round(15*viewScale_)),0,0,0,FW_NORMAL,
            FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,DEFAULT_PITCH | FF_DONTCARE,Theme::FontFamily);
        if (font) {
            SendMessage(nicknameInput_,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
            if (inputFont_) DeleteObject(inputFont_);
            inputFont_=font;
        }
        layoutNicknameInput();
    }
    const std::array<HWND,7> controls={actions_[0],actions_[1],actions_[2],nicknameInput_,updateDownloadButton_,cancelInstallationButton_,installationLogButton_};
    for(std::size_t i=0;i<controls.size();++i) if(controls[i]) {
        GetWindowRect(controls[i],&mainControlBounds_[i]);
        MapWindowPoints(nullptr,window_,reinterpret_cast<POINT*>(&mainControlBounds_[i]),2);
    }
    positionedScene_=-1;
}

float MainPage::pageOffset() const
{
    const float remaining=1-pageTravel_;
    const float logical=(settingsVisible_ ? 96.0F : -72.0F)*remaining*remaining*remaining;
    return viewScale_>0 ? std::round(logical*viewScale_)/viewScale_ : 0;
}

void MainPage::positionPageControls()
{
    const int pixels=static_cast<int>(std::round(pageOffset()*viewScale_));
    const int scene=settingsVisible_ ? 1 : 0;
    if(positionedScene_==scene && positionedOffset_==pixels) return;
    positionedScene_=scene;positionedOffset_=pixels;
    if(settingsVisible_) {settingsPage_.setTransitionOffset(pixels);return;}
    const std::array<HWND,7> controls={actions_[0],actions_[1],actions_[2],nicknameInput_,updateDownloadButton_,cancelInstallationButton_,installationLogButton_};
    auto batch=BeginDeferWindowPos(static_cast<int>(controls.size()));
    for(std::size_t i=0;i<controls.size();++i) {
        if(!controls[i]) continue;
        const auto& b=mainControlBounds_[i];
        if(batch) batch=DeferWindowPos(batch,controls[i],nullptr,b.left+pixels,b.top,0,0,
            SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
        if(!batch) break;
    }
    if(batch && EndDeferWindowPos(batch)) return;
    for(std::size_t i=0;i<controls.size();++i) if(controls[i])
        SetWindowPos(controls[i],nullptr,mainControlBounds_[i].left+pixels,mainControlBounds_[i].top,0,0,
            SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
}

void MainPage::syncControls()
{
    if(cancelInstallationButton_) {
        ShowWindow(cancelInstallationButton_,!settingsVisible_ && installation_.visible && installation_.running ? SW_SHOW : SW_HIDE);
        EnableWindow(cancelInstallationButton_,installation_.running && !installation_.cancelling);
        SetWindowTextW(cancelInstallationButton_,installation_.cancelling ? L"Cancelando instalação" : L"Cancelar instalação");
    }
    if(installationLogButton_) ShowWindow(installationLogButton_,!settingsVisible_ && installation_.visible ? SW_SHOW : SW_HIDE);
    if (updateDownloadButton_) {
        const bool busy = viewState_.progressVisible || (!viewState_.playEnabled && viewState_.operation != LauncherOperation::MinecraftRunning);
        ShowWindow(updateDownloadButton_, !settingsVisible_ && !busy && !updateWarning_.empty() ? SW_SHOW : SW_HIDE);
    }
    if (settingsVisible_) settingsPage_.setLocked(!viewState_.playEnabled || minecraftLauncherService_->isGameRunning());
    for (std::size_t i=0;i<actions_.size();++i) {
        if (!actions_[i]) continue;
        const bool enabled=!updatingLauncher_ && (i==3 || viewState_.playEnabled);
        if ((IsWindowEnabled(actions_[i]) != FALSE) != enabled) EnableWindow(actions_[i],enabled);
        if (!enabled) hovered_[i]=false;
    }
    if (nicknameInput_ && (IsWindowEnabled(nicknameInput_) != FALSE) != viewState_.playEnabled)
        EnableWindow(nicknameInput_,viewState_.playEnabled);
    const auto setLabel=[](HWND control,const std::wstring& label) {
        if (!control) return;
        std::array<wchar_t,256> current{};
        GetWindowText(control,current.data(),static_cast<int>(current.size()));
        if (label != current.data()) SetWindowText(control,label.c_str());
    };
    setLabel(actions_[0],originalMode_ ? L"Nickname local" : L"Nickname local (selecionado)");
    setLabel(actions_[1],originalMode_ ? L"Original (selecionado)" : L"Original");
    setLabel(actions_[2],viewState_.primaryActionLabel.empty() ? L"Jogar agora" : viewState_.primaryActionLabel);
}

void MainPage::updateMotionPreference()
{
    BOOL enabled=TRUE;
    if (SystemParametersInfo(SPI_GETCLIENTAREAANIMATION,0,&enabled,0)) motionEnabled_=enabled!=FALSE;
    if (!motionEnabled_) {
        for (std::size_t i=0;i<hover_.size();++i) hover_[i]=hovered_[i] ? 1.0F : 0.0F;
        displayedProgress_=static_cast<float>(viewState_.progressPercent);
        animationPhase_=0.45F;
        portalEntrance_=1;transitionTime_=1;leftTransition_.reset();
    }
}

void MainPage::redraw(bool animationFrame)
{
    if (!window_) return;
    // Any native interaction or new data invalidates the cached form. Timer-only frames reuse it.
    if(!animationFrame || !settingsVisible_ || pageTravel_==1 || !motionEnabled_ ||
        !IsWindowVisible(window_) || IsIconic(window_) || closing_) settingsTransitionFrame_.reset();
    const int scene=settingsVisible_ ? 2 : installation_.visible ? 1 : 0;
    if(scene!=0) portalHovered_=false;
    if(pulseAttempt_!=installation_.attempt) {
        pulseAttempt_=installation_.attempt;completionPulse_={};previousSteps_.fill(InstallationStepState::Waiting);
    }
    for(std::size_t i=0;i<completionPulse_.size();++i) {
        if(installation_.steps[i]!=previousSteps_[i] &&
            (installation_.steps[i]==InstallationStepState::Complete || installation_.steps[i]==InstallationStepState::Available))
            completionPulse_[i]=1;
        previousSteps_[i]=installation_.steps[i];
        if(!installation_.visible) completionPulse_[i]=0;
    }
    if(scene!=previousScene_) {
        leftTransition_.reset();transitionTime_=1;
        if(motionEnabled_ && frame_ && IsWindowVisible(window_) && !IsIconic(window_) &&
            scene!=2 && previousScene_!=2 && pageTravel_==1) {
            // Cache the pixels actually presented. Interrupted transitions start from that image.
            const int x=static_cast<int>(offsetX_+36*viewScale_),y=static_cast<int>(offsetY_+112*viewScale_);
            const int width=static_cast<int>(744*viewScale_),height=static_cast<int>(512*viewScale_);
            if(x>=0 && y>=0 && x+width<=static_cast<int>(frame_->GetWidth()) &&
                y+height<=static_cast<int>(frame_->GetHeight()) && width>0 && height>0) {
                leftTransition_.reset(frame_->Clone(Gdiplus::Rect(x,y,width,height),PixelFormat32bppPARGB));
                if(leftTransition_ && leftTransition_->GetLastStatus()==Gdiplus::Ok) {
                    transitionBounds_={static_cast<float>(x),static_cast<float>(y),static_cast<float>(width),static_cast<float>(height)};
                    transitionTime_=0;
                } else leftTransition_.reset();
            }
        }
        previousScene_=scene;
    }
    const auto& transfer=installation_.progress;
    const float fillTarget=static_cast<float>(transfer.percent.value_or(0));
    if(fillAttempt_!=installation_.attempt || fillStage_!=transfer.stage || fillTransfer_!=transfer.transferId ||
        transfer.activity!=InstallationActivity::Downloading || fillTarget<installationFill_) {
        installationFill_=0;
        fillAttempt_=installation_.attempt;fillStage_=transfer.stage;fillTransfer_=transfer.transferId;
    }
    frameDirty_ = true;
    if(!animationFrame) syncControls();
    positionPageControls();
    bool unsettled=std::abs(displayedProgress_-viewState_.progressPercent)>0.1F;
    for (std::size_t i=0;i<hover_.size();++i) unsettled |= std::abs(hover_[i]-(hovered_[i] ? 1.0F : 0.0F))>0.005F;
    for(std::size_t i=0;i<installationHover_.size();++i)
        unsettled |= std::abs(installationHover_[i]-(installationHovered_[i] ? 1.0F : 0.0F))>0.005F;
    const float selection=originalMode_ ? 1.0F : 0.0F;
    unsettled |= std::abs(identitySelection_-selection)>.005F;
    const bool transferVisible=installation_.visible && installation_.running && !installation_.cancelling &&
        transfer.activity==InstallationActivity::Downloading && transfer.percent.has_value();
    unsettled |= transferVisible && std::abs(installationFill_-fillTarget)>.1F;
    const float pressed=viewState_.playEnabled && actions_[2] &&
        (SendMessageW(actions_[2],BM_GETSTATE,0,0)&BST_PUSHED) ? 1.0F : 0.0F;
    unsettled |= std::abs(playPress_-pressed)>.005F || playSweep_<1 || playPulse_>0;
    unsettled |= std::abs(playLight_-playLightTarget_)>.005F;
    for(const float pulse:completionPulse_) unsettled |= pulse>0;
    const bool busy=viewState_.progressVisible ||
        (!viewState_.playEnabled && viewState_.operation!=LauncherOperation::MinecraftRunning);
    if (!motionEnabled_) {
        for (std::size_t i=0;i<hover_.size();++i) hover_[i]=hovered_[i] ? 1.0F : 0.0F;
        displayedProgress_=static_cast<float>(viewState_.progressPercent);
        for(std::size_t i=0;i<installationHover_.size();++i) installationHover_[i]=installationHovered_[i] ? 1.0F : 0.0F;
        identitySelection_=selection;installationFill_=fillTarget;
        portalEntrance_=1;transitionTime_=1;leftTransition_.reset();
        portalEnergy_=0;portalBurst_=0;playPress_=pressed;playPulse_=0;playSweep_=1;completionPulse_={};
        pageTravel_=1;playLight_=playLightTarget_;positionPageControls();
    }
    const bool indeterminate=installation_.visible ? installation_.running && !installation_.cancelling :
        busy && viewState_.progressPercent==0;
    const bool ambient=ambientMotion_ && !installation_.visible && GetForegroundWindow()==window_;
    const bool running=motionEnabled_ && IsWindowVisible(window_) && !IsIconic(window_) && !closing_ &&
        (settingsVisible_ ? pageTravel_<1 : pageTravel_<1 || unsettled || indeterminate || (!installation_.visible &&
            (ambient || portalEntrance_<1 || portalHovered_ || portalEnergy_>.005F || portalBurst_>0)) || transitionTime_<1);
    if (running && !animationTimer_) {
        previousFrame_=std::chrono::steady_clock::now();
        animationTimer_=SetTimer(window_,LauncherConstants::AnimationTimerId,USER_TIMER_MINIMUM,nullptr)!=0;
    } else if (!running && animationTimer_) {
        KillTimer(window_,LauncherConstants::AnimationTimerId); animationTimer_=false;
    }
    // Hover/focus changes the canvas and owner-drawn buttons, not the EDIT contents or caret.
    RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE | RDW_NOCHILDREN);
    if (settingsVisible_) settingsPage_.invalidateButtons();
    else {
        for (const auto action : actions_) if (action) InvalidateRect(action, nullptr, FALSE);
        if(cancelInstallationButton_) InvalidateRect(cancelInstallationButton_,nullptr,FALSE);
        if(installationLogButton_) InvalidateRect(installationLogButton_,nullptr,FALSE);
    }
}

void MainPage::animate()
{
    const auto now=std::chrono::steady_clock::now();
    const float dt=std::clamp(std::chrono::duration<float,std::milli>(now-previousFrame_).count(),0.0F,100.0F);
    previousFrame_=now;
    pageTravel_=std::min(1.0F,pageTravel_+dt/(settingsVisible_ ? 340.0F : 280.0F));
    playLight_+=(playLightTarget_-playLight_)*(1-std::exp(-dt/55));
    if(std::abs(playLight_-playLightTarget_)<.006F) playLight_=playLightTarget_;
    particlePhase_=std::fmod(particlePhase_+dt/8000,1.0F);
    const float energyTarget=portalHovered_ && !settingsVisible_ && !installation_.visible ? 1.0F : 0.0F;
    portalEnergy_+=(energyTarget-portalEnergy_)*(1-std::exp(-dt/95));
    if(std::abs(energyTarget-portalEnergy_)<.006F) portalEnergy_=energyTarget;
    portalBurst_=std::max(0.0F,portalBurst_-dt/1400);
    playSweep_=std::min(1.0F,playSweep_+dt/480);
    playPulse_=std::max(0.0F,playPulse_-dt/380);
    const float pressed=viewState_.playEnabled && actions_[2] &&
        (SendMessageW(actions_[2],BM_GETSTATE,0,0)&BST_PUSHED) ? 1.0F : 0.0F;
    playPress_+=(pressed-playPress_)*(1-std::exp(-dt/30));
    if(std::abs(pressed-playPress_)<.006F) playPress_=pressed;
    for(float& pulse:completionPulse_) pulse=std::max(0.0F,pulse-dt/420);
    const float ease=1-std::exp(-dt/35);
    for (std::size_t i=0;i<hover_.size();++i) {
        const float target=hovered_[i] ? 1.0F : 0.0F;
        hover_[i]+=(target-hover_[i])*ease;
        if (std::abs(target-hover_[i])<0.006F) hover_[i]=target;
    }
    const float target=static_cast<float>(viewState_.progressPercent);
    // A new phase can reset progress, so never animate a backwards value.
    if (target<displayedProgress_) displayedProgress_=target;
    else displayedProgress_+=(target-displayedProgress_)*(1-std::exp(-dt/85));
    if (std::abs(target-displayedProgress_)<0.1F) displayedProgress_=target;
    for(std::size_t i=0;i<installationHover_.size();++i) {
        const float targetHover=installationHovered_[i] ? 1.0F : 0.0F;
        installationHover_[i]+=(targetHover-installationHover_[i])*(1-std::exp(-dt/45));
        if(std::abs(targetHover-installationHover_[i])<.006F) installationHover_[i]=targetHover;
    }
    const float selection=originalMode_ ? 1.0F : 0.0F;
    identitySelection_+=(selection-identitySelection_)*(1-std::exp(-dt/55));
    if(std::abs(selection-identitySelection_)<.006F) identitySelection_=selection;
    const float fillTarget=static_cast<float>(installation_.progress.percent.value_or(0));
    installationFill_+=(fillTarget-installationFill_)*(1-std::exp(-dt/70));
    if(std::abs(fillTarget-installationFill_)<.1F) installationFill_=fillTarget;
    if(!settingsVisible_ && !installation_.visible) portalEntrance_=std::min(1.0F,portalEntrance_+dt/750);
    transitionTime_=std::min(1.0F,transitionTime_+dt/240);
    if(transitionTime_==1) leftTransition_.reset();
    animationPhase_+=dt/1600;
    animationPhase_=std::fmod(animationPhase_,1.0F);
    redraw(true);
}

LRESULT CALLBACK MainPage::controlProcedure(HWND control,UINT message,WPARAM wParam,
    LPARAM lParam,UINT_PTR id,DWORD_PTR data)
{
    auto* page=reinterpret_cast<MainPage*>(data);
    // WM_DRAWITEM covers the complete button, including its rounded corners.
    if (message == WM_ERASEBKGND) return 1;
    const auto index=static_cast<std::size_t>(id-1);
    if(index==2 && message==WM_MOUSEMOVE && IsWindowEnabled(control)) {
        RECT b{};GetClientRect(control,&b);
        const float light=b.right>0 ? std::clamp(static_cast<float>(GET_X_LPARAM(lParam))/b.right,0.0F,1.0F) : .5F;
        if(std::abs(page->playLightTarget_-light)>.005F) {page->playLightTarget_=light;page->redraw(true);}
    }
    if((id==11 || id==12) && (message==WM_MOUSEMOVE || message==WM_MOUSELEAVE)) {
        const auto slot=static_cast<std::size_t>(id-11);
        const bool hover=message==WM_MOUSEMOVE && IsWindowEnabled(control);
        if(page->installationHovered_[slot]!=hover) {
            page->installationHovered_[slot]=hover;
            if(hover) {TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,control,0};TrackMouseEvent(&track);}
            page->redraw();
        }
    }
    if (message==WM_MOUSEMOVE && index < page->hovered_.size() && !page->hovered_[index]) {
        page->hovered_[index]=true;
        if(index==2) page->playSweep_=0;
        TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT),TME_LEAVE,control,0};
        TrackMouseEvent(&track); page->redraw();
    } else if (message==WM_MOUSELEAVE && index < page->hovered_.size()) {
        page->hovered_[index]=false; page->redraw();
    } else if (message==WM_SETCURSOR && IsWindowEnabled(control)) {
        SetCursor(LoadCursor(nullptr,IDC_HAND)); return TRUE;
    } else if (message==WM_NCDESTROY) RemoveWindowSubclass(control,controlProcedure,id);
    if(index==2 && IsWindowEnabled(control)) {
        if(message==WM_SETFOCUS) page->playSweep_=0;
        if(message==WM_LBUTTONDOWN || (message==WM_KEYDOWN && wParam==VK_SPACE && !(lParam&(1LL<<30))))
            page->playPulse_=1;
    }
    const auto result=DefSubclassProc(control,message,wParam,lParam);
    if (message==WM_SETFOCUS || message==WM_KILLFOCUS ||
        message==WM_LBUTTONDOWN || message==WM_LBUTTONUP ||
        message==WM_KEYDOWN || message==WM_KEYUP) page->redraw();
    return result;
}

bool MainPage::handleKeyboard(MSG& message)
{
    if (settingsVisible_ && message.message == WM_KEYDOWN) {
        if (message.wParam == VK_ESCAPE) { hideSettings(); return true; }
        if (message.wParam == VK_RETURN) {
            const int id = settingsPage_.focusedControlId();
            if (id == SettingsPage::MinRam || id == SettingsPage::MaxRam || id == SettingsPage::Width ||
                id == SettingsPage::Height || id == SettingsPage::JavaPath || id == SettingsPage::GamePath) saveSettings();
            else if (SettingsPage::containsId(id) && id != SettingsPage::RamSlider) SendMessage(GetDlgItem(window_, id), BM_CLICK, 0, 0);
            return true;
        }
    }
    if (message.message==WM_KEYDOWN && message.wParam==VK_RETURN) {
        for(const auto action:{cancelInstallationButton_,installationLogButton_}) if(action && GetFocus()==action) {
            SendMessageW(action,BM_CLICK,0,0);return true;
        }
        if (GetFocus()==updateDownloadButton_) { SendMessage(updateDownloadButton_,BM_CLICK,0,0); return true; }
        if (GetFocus()==nicknameInput_) { handlePrimaryAction(); return true; }
        for (const auto action : actions_) if (GetFocus()==action) {
            SendMessage(action,BM_CLICK,0,0); return true;
        }
    }
    return IsDialogMessage(window_,&message)!=FALSE;
}

bool MainPage::renderPreviews(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory,error);
    if (error) return false;
    UINT count=0,size=0;
    Gdiplus::GetImageEncodersSize(&count,&size);
    if (!size) return false;
    std::vector<BYTE> bytes(size);
    auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());
    if (Gdiplus::GetImageEncoders(count,size,encoders)!=Gdiplus::Ok) return false;
    CLSID png{};
    bool found=false;
    for (UINT i=0;i<count;++i) if (std::wstring_view(encoders[i].MimeType)==L"image/png") {
        png=encoders[i].Clsid; found=true; break;
    }
    if (!found) return false;
    const auto initial=viewState_;
    for (int variant=0;variant<24;++variant) {
        viewState_=initial; originalMode_=false; updatingLauncher_=false; updateWarning_.clear();
        installation_={};
        const wchar_t* filename=L"loading.png";
        if (variant!=0) {
            viewState_.statusCards={
                {L"INSTALAÇÃO",L"Pronta",L"Minecraft e Forge preparados",StatusTone::Success},
                {L"ARQUIVOS",L"Sincronizados",L"Manifesto privado conferido",StatusTone::Success},
                {L"SERVIDOR HTTP",L"Disponível",L"Manifesto de arquivos acessível",StatusTone::Success}};
            viewState_.operation=LauncherOperation::ReadyToPlay;
            viewState_.headline=L"Pronto para jogar";
            viewState_.operationMessage=L"Seu mundo está preparado. Boa aventura!";
            viewState_.primaryActionLabel=L"JOGAR AGORA";
            viewState_.playEnabled=true;
            filename=L"desktop.png";
        }
        if (variant==2) {
            filename=L"original.png"; originalMode_=true;
        } else if (variant==3) {
            filename=L"download.png";
            viewState_.operation=LauncherOperation::DownloadingFiles;
            viewState_.headline=L"Preparando Minecraft";
            viewState_.operationMessage=L"Baixando e verificando os arquivos do modpack...";
            viewState_.primaryActionLabel=L"PREPARANDO...";
            viewState_.playEnabled=false; viewState_.progressVisible=true; viewState_.progressPercent=47;
        } else if (variant==4) {
            filename=L"error.png";
            viewState_.operation=LauncherOperation::ServerUnavailable;
            viewState_.headline=L"Servidor indisponível";
            viewState_.operationMessage=L"Não foi possível consultar o manifesto. Verifique sua conexão e tente novamente.";
            viewState_.primaryActionLabel=L"TENTAR NOVAMENTE";
            viewState_.statusCards[2]={L"SERVIDOR HTTP",L"Indisponível",L"Falha na consulta do manifesto",StatusTone::Error};
        } else if (variant==5) filename=L"compact.png";
        else if (variant==6) {
            filename=L"first-install.png";
            viewState_.operation=LauncherOperation::NeedsInstallation;
            viewState_.headline=L"Pronto para instalar";
            viewState_.operationMessage=L"Clique em Jogar para preparar Java, Minecraft e Forge.";
            viewState_.primaryActionLabel=L"INSTALAR E JOGAR";
            viewState_.statusCards[0]={L"INSTALAÇÃO",L"Incompleta",L"Java 17 será baixado automaticamente",StatusTone::Warning};
        } else if (variant==7) {
            filename=L"auth-error.png"; originalMode_=true;
            viewState_.operation=LauncherOperation::AuthenticationError;
            viewState_.headline=L"Falha no login Microsoft";
            viewState_.operationMessage=L"Não foi possível autenticar. Confira sua conta e tente novamente.";
            viewState_.primaryActionLabel=L"TENTAR NOVAMENTE";
        } else if (variant==8) {
            filename=L"nickname-error.png";
            viewState_.operation=LauncherOperation::InvalidNickname;
            viewState_.headline=L"Confira seu nickname";
            viewState_.operationMessage=L"Use de 3 a 16 letras, números ou underline antes de instalar.";
            viewState_.primaryActionLabel=L"TENTAR NOVAMENTE";
        } else if (variant==9) {
            filename=L"sign-in.png"; originalMode_=true;
            viewState_.operation=LauncherOperation::SigningIn;
            viewState_.headline=L"Entrando com Microsoft";
            viewState_.operationMessage=L"Aguarde o código de confirmação...";
            viewState_.primaryActionLabel=L"CONECTANDO...";
            viewState_.playEnabled=false;
        }
        if (variant>=10 && variant<=13) {
            updatingLauncher_=true;
            applyLauncherUpdateProgress({static_cast<LauncherUpdatePhase>(variant-10),variant==10 ? 0 : variant==11 ? 47 : 100});
            const wchar_t* names[]={L"update-check.png",L"update-download.png",L"update-apply.png",L"update-restart.png"};
            filename=names[variant-10];
        } else if(variant==15) {
            updatingLauncher_=true; applyLauncherUpdateProgress({LauncherUpdatePhase::Downloading,47});
            filename=L"update-compact.png";
        } else if(variant==14 || variant==16) {
            updateWarning_=L"A atualização falhou. A versão anterior foi restaurada.";
            filename=variant==14 ? L"update-recovery.png" : L"update-recovery-compact.png";
        }
        if(variant>=17) {
            installation_.begin();
            auto& value=installation_.progress;
            value.message=L"Consultando os arquivos do grupo...";
            viewState_.operation=LauncherOperation::PreparingMinecraft;
            viewState_.playEnabled=false;viewState_.progressVisible=true;viewState_.progressPercent=0;
            viewState_.headline=L"Preparando seu mundo";viewState_.operationMessage=value.message;
            viewState_.primaryActionLabel=L"PREPARANDO...";
            filename=L"installation-checking.png";
            if(variant!=17) {
                installation_.steps[0]=InstallationStepState::Available;
                installation_.steps[1]=InstallationStepState::Active;
                value.stage=InstallationStage::Minecraft;value.activity=InstallationActivity::Downloading;
                value.message=L"Baixando Minecraft, bibliotecas e assets...";
                value.receivedBytes=282ULL*1024*1024;value.totalBytes=600ULL*1024*1024;value.percent=47;
                value.completedFiles=140;value.totalFiles=900;value.reusedFiles=620;
                const auto now=InstallationTransferMeter::Clock::now();
                installation_.meter.sample(value.receivedBytes-12ULL*1024*1024,now-std::chrono::seconds(3));
                installation_.meter.sample(value.receivedBytes,now);
                viewState_.headline=L"Minecraft e assets";viewState_.operationMessage=value.message;viewState_.progressPercent=47;
                filename=variant==23 ? L"installation-compact.png" : L"installation-download.png";
            }
            if(variant==19) {
                value={InstallationStage::Forge,InstallationActivity::Installing,InstallationStepState::Active,L"Executando instalador Forge 47.4.10..."};
                installation_.steps[1]=InstallationStepState::Complete;installation_.steps[2]=InstallationStepState::Complete;
                installation_.steps[3]=InstallationStepState::Active;
                viewState_.headline=L"Forge";viewState_.operationMessage=value.message;viewState_.progressPercent=0;
                filename=L"installation-forge.png";
            } else if(variant==20 || variant==21) {
                const bool cancelled=variant==21;
                installation_.finish(installation_.attempt,{cancelled ? InstallationOutcome::Cancelled : InstallationOutcome::Failed,
                    cancelled ? InstallationFailure::None : InstallationFailure::Network,
                    cancelled ? L"Instalação cancelada. Os arquivos completos serão reaproveitados." : L"A conexão foi interrompida. Confira sua internet e tente novamente."});
                viewState_.operation=cancelled ? LauncherOperation::NeedsInstallation : LauncherOperation::LaunchError;
                viewState_.headline=cancelled ? L"Instalação cancelada" : L"Falha na preparação";
                viewState_.operationMessage=installation_.result->message;viewState_.playEnabled=true;viewState_.progressVisible=false;viewState_.progressPercent=0;
                viewState_.primaryActionLabel=cancelled ? L"CONTINUAR INSTALAÇÃO" : L"TENTAR NOVAMENTE";
                filename=cancelled ? L"installation-cancelled.png" : L"installation-error.png";
            } else if(variant==22) {
                installation_.cancelling=true;
                viewState_.headline=L"Cancelando instalação...";viewState_.operationMessage=L"Aguardando as tarefas encerrarem com segurança...";
                viewState_.primaryActionLabel=L"CANCELANDO...";viewState_.progressPercent=0;
                filename=L"installation-cancelling.png";
            }
        }
        displayedProgress_=static_cast<float>(viewState_.progressPercent); animationPhase_=0.45F;
        viewScale_=variant==5 || variant==15 || variant==16 || variant==23 ? 0.8F : 1;
        Gdiplus::Bitmap frame(static_cast<INT>(LauncherConstants::WindowWidth*viewScale_),
            static_cast<INT>(LauncherConstants::WindowHeight*viewScale_),PixelFormat32bppPARGB);
        Gdiplus::Graphics graphics(&frame);
        render(graphics,true);
        if (frame.Save((directory/filename).c_str(),&png)!=Gdiplus::Ok) return false;
    }
    installation_={};updatingLauncher_=false; updateWarning_.clear();
    settingsVisible_ = true;
    for (int variant = 0; variant < 4; ++variant) {
        settingsPage_.open(LauncherSettings{}, {});
        if (variant == 2) {
            LauncherSettings invalid;
            invalid.minimumMemoryMegabytes = 8192;
            settingsPage_.open(invalid, {});
            std::wstring validation;
            LauncherSettingsStore::validate(invalid, validation, false);
            settingsPage_.setMessage(validation, true);
        }
        if (variant == 3) {
            LauncherSettings halfStep;
            halfStep.maximumMemoryMegabytes = 6656;
            settingsPage_.open(halfStep, {});
        }
        viewScale_ = variant == 1 ? 0.8F : 1.0F;
        Gdiplus::Bitmap frame(static_cast<INT>(LauncherConstants::WindowWidth * viewScale_),
            static_cast<INT>(LauncherConstants::WindowHeight * viewScale_), PixelFormat32bppPARGB);
        Gdiplus::Graphics graphics(&frame);
        render(graphics, true);
        const auto filename = variant == 0 ? L"settings.png" : variant == 1 ? L"settings-compact.png" : variant == 2 ? L"settings-error.png" : L"settings-half-step.png";
        if (frame.Save((directory / filename).c_str(), &png) != Gdiplus::Ok) return false;
    }
    settingsVisible_ = false;
    viewState_=initial; originalMode_=false; viewScale_=1;
    return true;
}
