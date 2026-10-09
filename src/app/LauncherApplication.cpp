#include "app/LauncherApplication.h"

#include "pages/MainPage.h"
#include "utils/DrawingUtils.h"
#include "services/LauncherUpdateService.h"

#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <shellapi.h>

LauncherApplication::LauncherApplication(HINSTANCE instance)
    : instance_(instance)
{
}

int LauncherApplication::run(int showCommand)
{
    int argumentCount = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    struct ArgumentsOwner { wchar_t** value; ~ArgumentsOwner() {if(value) LocalFree(value);} } argumentsOwner{arguments};
    if (!arguments) return 1;
    if (const auto helper = LauncherUpdateRuntime::dispatch(argumentCount, arguments)) return *helper;
    const bool preview = argumentCount == 3 && std::wstring_view(arguments[1]) == L"--render-preview";
    struct InstanceMutex {
        HANDLE handle = nullptr;
        bool acquired = false;
        ~InstanceMutex() {if(acquired) ReleaseMutex(handle); if(handle) CloseHandle(handle);}
    } instanceMutex;
    bool skipUpdate = false;
    std::wstring updateWarning;
    if (!preview) {
        const auto name = LauncherUpdateRuntime::mutexName(LauncherUpdateRuntime::executablePath(), false);
        instanceMutex.handle = CreateMutexW(nullptr, FALSE, name.c_str());
        if (instanceMutex.handle) {
            const auto wait = WaitForSingleObject(instanceMutex.handle, 0);
            instanceMutex.acquired = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        }
        if (!instanceMutex.acquired) {
            MessageBoxW(nullptr, L"Este launcher já está aberto ou concluindo uma atualização.", L"Eternal Launcher", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (!LauncherUpdateRuntime::startup(argumentCount, arguments, skipUpdate, updateWarning)) {
            MessageBoxW(nullptr, updateWarning.empty() ? L"Não foi possível recuperar a atualização." : updateWarning.c_str(),
                L"Eternal Launcher", MB_OK | MB_ICONINFORMATION);
            return 1;
        }
    }
    // O token mantém o GDI+ ativo durante toda a vida da interface.
    Gdiplus::GdiplusStartupInput startupInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
    {
        MessageBox(nullptr, L"Não foi possível inicializar a interface gráfica.",
                   L"Eternal Launcher", MB_OK | MB_ICONERROR);
        return 1;
    }

    auto displayTypeface = std::make_unique<DrawingUtils::DisplayTypeface>();
    auto mainPage = std::make_unique<MainPage>(instance_);
    if (preview) {
        const bool rendered = mainPage->renderPreviews(arguments[2]);
        mainPage.reset();
        displayTypeface.reset();
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return rendered ? 0 : 1;
    }
    if (!mainPage->create(showCommand, !skipUpdate, std::move(updateWarning)))
    {
        mainPage.reset();
        displayTypeface.reset();
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }
    LauncherUpdateRuntime::windowReady(argumentCount, arguments);

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0)
    {
        if (mainPage->handleKeyboard(message)) continue;
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    mainPage.reset();
    displayTypeface.reset();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return static_cast<int>(message.wParam);
}
