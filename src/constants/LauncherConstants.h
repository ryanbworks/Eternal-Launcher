#pragma once

#include <windows.h>

namespace LauncherConstants
{
    inline constexpr wchar_t WindowClassName[] = L"EternalLauncherMainWindow";
    inline constexpr wchar_t WindowTitle[] = L"Eternal Launcher";
    inline constexpr int WindowWidth = 1180;
    inline constexpr int WindowHeight = 730;
    inline constexpr int MinimumWindowWidth = 1020;
    inline constexpr int MinimumWindowHeight = 680;

    inline constexpr int NicknameInputId = 1001;
    inline constexpr UINT_PTR GameProcessTimerId = 2001;
    inline constexpr UINT FileServerResultMessage = WM_APP + 1;
    inline constexpr UINT JavaRuntimeProgressMessage = WM_APP + 2;
    inline constexpr UINT JavaRuntimeResultMessage = WM_APP + 3;
    inline constexpr UINT GameInstallationProgressMessage = WM_APP + 4;
    inline constexpr UINT GameInstallationResultMessage = WM_APP + 5;
}
