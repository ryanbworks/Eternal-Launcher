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
    CheckingInstallation,
    CheckingMods,
    CheckingUpdates,
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
    NewsItem featuredNews;
};
