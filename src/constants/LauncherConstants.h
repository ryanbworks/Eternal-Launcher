#pragma once

#include <windows.h>

namespace LauncherConstants
{
    inline constexpr wchar_t WindowClassName[] = L"EternalLauncherMainWindow";
    inline constexpr wchar_t WindowTitle[] = L"Eternal Launcher";
    // Logical canvas. The whole interface scales uniformly from this size.
    inline constexpr int WindowWidth = 1280;
    inline constexpr int WindowHeight = 800;
    inline constexpr int MinimumWindowWidth = 1024;
    inline constexpr int MinimumWindowHeight = 640;
    inline constexpr float TitleBarHeight = 44.0F;
    inline constexpr float SidebarWidth = 76.0F;

    inline constexpr int NicknameInputId = 1001;
    inline constexpr int PlayButtonId = 1002;
    inline constexpr int OfflineButtonId = 1003;
    inline constexpr int OriginalButtonId = 1004;
    inline constexpr int UpdateDownloadButtonId = 1006;
    inline constexpr int CancelInstallationButtonId = 1007;
    inline constexpr int InstallationLogButtonId = 1008;

    // Window chrome.
    inline constexpr int MinimizeButtonId = 1101;
    inline constexpr int MaximizeButtonId = 1102;
    inline constexpr int CloseButtonId = 1103;
    // Sidebar navigation.
    inline constexpr int NavHomeId = 1111;
    inline constexpr int NavNewsId = 1112;
    inline constexpr int NavModsId = 1113;
    inline constexpr int NavSettingsId = 1114;
    inline constexpr int NavHelpId = 1115;
    // Home.
    inline constexpr int PlayMenuButtonId = 1121;
    inline constexpr int EditNicknameButtonId = 1122;
    inline constexpr int QuickFolderButtonId = 1123;
    inline constexpr int QuickRepairButtonId = 1124;
    inline constexpr int QuickLogsButtonId = 1125;
    inline constexpr int NewsCardFirstId = 1126;
    inline constexpr int NewsCardCount = 3;
    inline constexpr int NewsAllButtonId = 1129;
    inline constexpr int ToastNewsButtonId = 1130;
    // News page.
    inline constexpr int NewsLinkFirstId = 1141;
    inline constexpr int NewsLinkCount = 4;
    // Mods page.
    inline constexpr int ModsFolderButtonId = 1151;
    inline constexpr int ModsAddButtonId = 1152;
    inline constexpr int ModsRefreshButtonId = 1153;
    // Help page.
    inline constexpr int HelpRepairButtonId = 1161;
    inline constexpr int HelpInstallLogButtonId = 1162;
    inline constexpr int HelpForgeLogButtonId = 1163;
    inline constexpr int HelpGameLogButtonId = 1164;
    inline constexpr int HelpLogsFolderButtonId = 1165;
    inline constexpr int HelpCopySupportButtonId = 1166;
    inline constexpr int HelpReportIssueButtonId = 1167;
    inline constexpr int HelpDataFolderButtonId = 1168;

    // Popup and tray menu commands.
    inline constexpr UINT MenuOpenGameFolder = 40001;
    inline constexpr UINT MenuOpenModsFolder = 40002;
    inline constexpr UINT MenuRepair = 40003;
    inline constexpr UINT MenuOpenLogs = 40004;
    inline constexpr UINT MenuTrayOpen = 40011;
    inline constexpr UINT MenuTrayExit = 40012;

    inline constexpr UINT_PTR GameProcessTimerId = 2001;
    inline constexpr UINT_PTR AnimationTimerId = 2002;
    inline constexpr UINT_PTR InstallationProgressTimerId = 2003;
    inline constexpr UINT_PTR PlaytimeTimerId = 2004;

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
    inline constexpr UINT InstallationProgressMessage = WM_APP + 11;
    inline constexpr UINT NewsResultMessage = WM_APP + 12;
    inline constexpr UINT TrayIconMessage = WM_APP + 13;
}
