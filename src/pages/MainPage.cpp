#include "pages/MainPage.h"

#include "constants/LauncherConstants.h"
#include "mocks/MockLauncherData.h"
#include "styles/Theme.h"

#include <dwmapi.h>
#include <gdiplus.h>

MainPage::MainPage(HINSTANCE instance)
    : instance_(instance),
      backgroundBrush_(CreateSolidBrush(RGB(11, 13, 18))),
      viewState_(MockLauncherData::createInitialState())
{
}

MainPage::~MainPage()
{
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

    RECT desiredRect{0, 0, LauncherConstants::WindowWidth, LauncherConstants::WindowHeight};
    AdjustWindowRect(&desiredRect, WS_OVERLAPPEDWINDOW, FALSE);

    window_ = CreateWindowEx(
        0,
        LauncherConstants::WindowClassName,
        LauncherConstants::WindowTitle,
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
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
    case WM_DESTROY:
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

    Gdiplus::Graphics graphics(deviceContext);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.Clear(Theme::Background);

    Gdiplus::Font titleFont(Theme::FontFamily, 32.0F, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush titleBrush(Theme::TextPrimary);
    graphics.DrawString(L"ETERNAL", -1, &titleFont, Gdiplus::PointF(40.0F, 38.0F), &titleBrush);

    Gdiplus::Font subtitleFont(Theme::FontFamily, 16.0F, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush subtitleBrush(Theme::TextSecondary);
    graphics.DrawString(L"Estrutura visual em construção", -1, &subtitleFont,
                        Gdiplus::PointF(42.0F, 88.0F), &subtitleBrush);

    EndPaint(window_, &paintStruct);
}

void MainPage::enableDarkTitleBar() const
{
    // Integra a moldura nativa ao tema escuro quando a versão do Windows oferece suporte.
    constexpr DWORD useImmersiveDarkMode = 20;
    BOOL enabled = TRUE;
    DwmSetWindowAttribute(window_, useImmersiveDarkMode, &enabled, sizeof(enabled));
}
