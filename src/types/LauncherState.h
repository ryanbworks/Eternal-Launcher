#pragma once

#include <string>
#include <vector>

enum class StatusTone
{
    Success,
    Information,
    Warning,
    Error,
    Neutral
};

enum class LauncherOperation
{
    Idle,
    NeedsInstallation,
    SigningIn,
    AuthenticationError,
    InvalidNickname,
    CheckingInstallation,
    CheckingMods,
    CheckingUpdates,
    CheckingLauncherUpdate,
    DownloadingLauncherUpdate,
    ApplyingLauncherUpdate,
    RestartingLauncher,
    DownloadingFiles,
    UpdatingMods,
    PreparingMinecraft,
    ReadyToPlay,
    ConnectionError,
    DownloadError,
    CorruptedFile,
    ServerUnavailable,
    UpdateAvailable,
    UpdateCompleted,
    LaunchingMinecraft,
    MinecraftRunning,
    LaunchError
};

inline bool isLauncherError(LauncherOperation operation)
{
    return operation == LauncherOperation::ConnectionError ||
           operation == LauncherOperation::DownloadError ||
           operation == LauncherOperation::CorruptedFile ||
           operation == LauncherOperation::ServerUnavailable ||
           operation == LauncherOperation::LaunchError ||
           operation == LauncherOperation::AuthenticationError ||
           operation == LauncherOperation::InvalidNickname;
}

struct StatusCardData
{
    std::wstring label;
    std::wstring value;
    std::wstring detail;
    StatusTone tone = StatusTone::Neutral;
};

struct NewsItem
{
    std::wstring category;
    std::wstring title;
    std::wstring description;
    std::wstring date;
    std::wstring url;
    // Written into the launcher itself (release notes), not published by the server.
    bool local = false;
};

struct LauncherViewState
{
    LauncherOperation operation = LauncherOperation::Idle;
    std::wstring headline;
    std::wstring operationMessage;
    std::wstring primaryActionLabel;
    int progressPercent = 0;
    bool progressVisible = false;
    bool playEnabled = true;
    std::vector<StatusCardData> statusCards;
    std::vector<NewsItem> news;
};
