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
    inline constexpr UINT_PTR MockProgressTimerId = 2001;
}
