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
    inline constexpr UINT_PTR AnimationTimerId = 2002;
    inline constexpr int PlayButtonId = 1002;
    inline constexpr int OfflineButtonId = 1003;
    inline constexpr int OriginalButtonId = 1004;
    inline constexpr int SettingsButtonId = 1005;
    inline constexpr int UpdateDownloadButtonId = 1006;
    inline constexpr int CancelInstallationButtonId = 1007;
    inline constexpr int InstallationLogButtonId = 1008;
    inline constexpr UINT_PTR InstallationProgressTimerId = 2003;
    inline constexpr UINT InstallationProgressMessage = WM_APP + 11;
    inline constexpr UINT FileServerResultMessage = WM_APP + 1;
    inline constexpr UINT JavaRuntimeProgressMessage = WM_APP + 2;
    inline constexpr UINT JavaRuntimeResultMessage = WM_APP + 3;
    inline constexpr UINT GameInstallationProgressMessage = WM_APP + 4;
    inline constexpr UINT GameInstallationResultMessage = WM_APP + 5;
    inline constexpr UINT MicrosoftDeviceCodeMessage = WM_APP + 6;
    inline constexpr UINT MicrosoftAuthResultMessage = WM_APP + 7;
    inline constexpr UINT InterfaceRedrawMessage = WM_APP + 8;
    inline constexpr UINT LauncherUpdateProgressMessage = WM_APP + 9;
    inline constexpr UINT LauncherUpdateResultMessage = WM_APP + 10;
}
