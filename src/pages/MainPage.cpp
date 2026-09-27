#include "pages/MainPage.h"

#include "config/LauncherConfig.h"
#include "constants/LauncherConstants.h"
#include "mocks/MockLauncherData.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <array>
#include <string>

namespace
{
    const Gdiplus::RectF HeroBounds(32.0F, 104.0F, 720.0F, 260.0F);
    const Gdiplus::RectF StatusBounds(32.0F, 380.0F, 720.0F, 128.0F);
    const Gdiplus::RectF NewsBounds(32.0F, 524.0F, 720.0F, 166.0F);
    const Gdiplus::RectF ProfileBounds(772.0F, 104.0F, 376.0F, 174.0F);
    const Gdiplus::RectF PlayBounds(772.0F, 294.0F, 376.0F, 214.0F);
    const Gdiplus::RectF ProgressBounds(772.0F, 524.0F, 376.0F, 166.0F);
}

MainPage::MainPage(HINSTANCE instance)
    : instance_(instance),
      backgroundBrush_(CreateSolidBrush(RGB(11, 13, 18))),
      inputBrush_(CreateSolidBrush(RGB(25, 29, 39))),
      viewState_(MockLauncherData::createInitialState())
{
}

MainPage::~MainPage()
{
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

bool MainPage::create(int showCommand)
{
    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = MainPage::windowProcedure;
    windowClass.lpszClassName = LauncherConstants::WindowClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = backgroundBrush_;
    windowClass.style = CS_HREDRAW | CS_VREDRAW;

    if (RegisterClassEx(&windowClass) == 0)
    {
        return false;
    }

    // A primeira versão possui layout fixo para preservar ritmo e alinhamento.
    // Uma estratégia responsiva poderá ser adicionada quando surgirem novas páginas.
    constexpr DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT desiredRect{0, 0, LauncherConstants::WindowWidth, LauncherConstants::WindowHeight};
    AdjustWindowRect(&desiredRect, windowStyle, FALSE);

    window_ = CreateWindowEx(
        0,
        LauncherConstants::WindowClassName,
        LauncherConstants::WindowTitle,
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        desiredRect.right - desiredRect.left,
        desiredRect.bottom - desiredRect.top,
        nullptr,
        nullptr,
        instance_,
        this);

    if (window_ == nullptr)
    {
        return false;
    }

    enableDarkTitleBar();
    createNicknameInput();
    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
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
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONUP:
    {
        const float x = static_cast<float>(GET_X_LPARAM(lParam));
        const float y = static_cast<float>(GET_Y_LPARAM(lParam));
        if (DrawingUtils::contains(playPanel_.buttonBounds(PlayBounds), x, y))
        {
            handlePrimaryAction();
        }
        else if (DrawingUtils::contains(header_.settingsBounds(
                     static_cast<float>(LauncherConstants::WindowWidth)), x, y))
        {
            showSettingsPlaceholder();
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == LauncherConstants::MockProgressTimerId)
        {
            advanceMockProgress();
        }
        return 0;
    case WM_CTLCOLOREDIT:
    {
        const HDC editContext = reinterpret_cast<HDC>(wParam);
        SetTextColor(editContext, RGB(245, 247, 250));
        SetBkColor(editContext, RGB(25, 29, 39));
        return reinterpret_cast<LRESULT>(inputBrush_);
    }
    case WM_DESTROY:
        KillTimer(window_, LauncherConstants::MockProgressTimerId);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(window_, message, wParam, lParam);
    }
}

void MainPage::paint()
{
    PAINTSTRUCT paintStruct{};
    HDC deviceContext = BeginPaint(window_, &paintStruct);

    // O buffer intermediário evita cintilação enquanto todos os componentes são redesenhados.
    Gdiplus::Bitmap frame(LauncherConstants::WindowWidth, LauncherConstants::WindowHeight,
                          PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&frame);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    graphics.Clear(Theme::Background);

    // Brilhos discretos dão profundidade ao fundo sem exigir imagens externas.
    Gdiplus::SolidBrush topGlow(Gdiplus::Color(18, 139, 92, 246));
    graphics.FillEllipse(&topGlow, 730.0F, -300.0F, 700.0F, 520.0F);

    header_.draw(graphics, static_cast<float>(LauncherConstants::WindowWidth));
    hero_.draw(graphics, HeroBounds);
    statusPanel_.draw(graphics, StatusBounds, viewState_.statusCards);
    newsPanel_.draw(graphics, NewsBounds, viewState_.featuredNews);
    playerProfile_.draw(graphics, ProfileBounds);
    playPanel_.draw(graphics, PlayBounds, viewState_);
    progress_.draw(graphics, ProgressBounds, viewState_.progressPercent,
                   viewState_.progressVisible
                       ? viewState_.operationMessage
                       : L"Mods e arquivos locais estão sincronizados.",
                   viewState_.progressVisible);

    Gdiplus::Graphics screen(deviceContext);
    screen.DrawImage(&frame, 0, 0);
    EndPaint(window_, &paintStruct);
}

void MainPage::enableDarkTitleBar() const
{
    // Integra a moldura nativa ao tema escuro quando a versão do Windows oferece suporte.
    constexpr DWORD useImmersiveDarkMode = 20;
    BOOL enabled = TRUE;
    DwmSetWindowAttribute(window_, useImmersiveDarkMode, &enabled, sizeof(enabled));
}

void MainPage::createNicknameInput()
{
    nicknameInput_ = CreateWindowEx(
        0,
        L"EDIT",
        LauncherConfig::DefaultNickname.data(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(LauncherConstants::NicknameInputId)),
        instance_,
        nullptr);

    inputFont_ = CreateFont(
        -18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
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
                 static_cast<int>(field.X + 14.0F), static_cast<int>(field.Y + 10.0F),
                 static_cast<int>(field.Width - 28.0F), static_cast<int>(field.Height - 18.0F),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainPage::handlePrimaryAction()
{
    if (!viewState_.playEnabled)
    {
        return;
    }

    std::array<wchar_t, 32> nickname{};
    GetWindowText(nicknameInput_, nickname.data(), static_cast<int>(nickname.size()));
    if (std::wstring_view(nickname.data()).empty())
    {
        viewState_.headline = L"Informe seu nickname";
        viewState_.operationMessage = L"Precisamos saber como identificar você no jogo.";
        InvalidateRect(window_, nullptr, FALSE);
        SetFocus(nicknameInput_);
        return;
    }

    // MOCK: demonstra visualmente o fluxo futuro sem acessar rede, arquivos ou Minecraft.
    viewState_.operation = LauncherOperation::CheckingInstallation;
    viewState_.headline = L"Verificando instalação";
    viewState_.operationMessage = L"Validando a estrutura local simulada...";
    viewState_.progressPercent = 8;
    viewState_.progressVisible = true;
    viewState_.playEnabled = false;
    mockProgressStep_ = 0;
    SetTimer(window_, LauncherConstants::MockProgressTimerId, 720, nullptr);
    InvalidateRect(window_, nullptr, FALSE);
}

void MainPage::advanceMockProgress()
{
    // MOCK: sequência temporal usada somente para validar estados, textos e barra de progresso.
    static const auto steps = MockLauncherData::createProgressSequence();

    if (mockProgressStep_ >= static_cast<int>(steps.size()))
    {
        KillTimer(window_, LauncherConstants::MockProgressTimerId);
        return;
    }

    const auto& step = steps[static_cast<std::size_t>(mockProgressStep_)];
    viewState_.operation = step.operation;
    viewState_.progressPercent = step.progress;
    viewState_.headline = step.headline;
    viewState_.operationMessage = step.message;
    ++mockProgressStep_;

    if (mockProgressStep_ >= static_cast<int>(steps.size()))
    {
        KillTimer(window_, LauncherConstants::MockProgressTimerId);
        viewState_.playEnabled = true;
        viewState_.progressVisible = false;
    }

    InvalidateRect(window_, nullptr, FALSE);
}

void MainPage::showSettingsPlaceholder() const
{
    MessageBox(
        window_,
        L"As configurações serão adicionadas em uma etapa futura.\n\n"
        L"Nenhuma preferência foi alterada.",
        L"Configurações — em breve",
        MB_OK | MB_ICONINFORMATION);
}
