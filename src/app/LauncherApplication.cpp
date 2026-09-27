#include "app/LauncherApplication.h"

#include "pages/MainPage.h"

#include <objidl.h>
#include <gdiplus.h>
#include <memory>

LauncherApplication::LauncherApplication(HINSTANCE instance)
    : instance_(instance)
{
}

int LauncherApplication::run(int showCommand)
{
    // O token mantém o GDI+ ativo durante toda a vida da interface.
    Gdiplus::GdiplusStartupInput startupInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
    {
        MessageBox(nullptr, L"Não foi possível inicializar a interface gráfica.",
                   L"Eternal Launcher", MB_OK | MB_ICONERROR);
        return 1;
    }

    auto mainPage = std::make_unique<MainPage>(instance_);
    if (!mainPage->create(showCommand))
    {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    mainPage.reset();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return static_cast<int>(message.wParam);
}
