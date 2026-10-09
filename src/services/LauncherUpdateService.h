#pragma once
#include "services/LauncherUpdateTransport.h"
#include "services/LauncherUpdateManifest.h"
#include <windows.h>
#include <functional>
#include <optional>

enum class LauncherUpdatePhase { Checking, Downloading, Applying, Restarting };
struct LauncherUpdateProgress { LauncherUpdatePhase phase; int percent = 0; };
struct LauncherUpdateResult {
    bool restart = false;
    bool updateAvailable = false;
    std::wstring version;
    std::wstring codename;
    std::wstring warning;
};
using LauncherUpdateProgressCallback = std::function<void(const LauncherUpdateProgress&)>;

class LauncherUpdateService
{
public:
    // prepare() never closes the caller. A successful handoff asks the UI to close normally.
    LauncherUpdateResult prepare(const std::filesystem::path& executable, std::stop_token stop,
        const LauncherUpdateProgressCallback& progress, const ILauncherUpdateTransport& transport,
        bool confirmed = true) const;
};

// Process coordination also runs without GDI+, before normal application startup.
namespace LauncherUpdateRuntime
{
    std::filesystem::path executablePath();
    std::filesystem::path transactionDirectory(const std::filesystem::path& executable);
    std::wstring mutexName(const std::filesystem::path& executable, bool update);
    std::optional<int> dispatch(int count, wchar_t** arguments);
    // Called after acquiring the ordinary single-instance mutex.
    bool startup(int count, wchar_t** arguments, bool& skipCheck, std::wstring& warning);
    void windowReady(int count, wchar_t** arguments);
    bool validateExecutable(const std::filesystem::path& file, const LauncherRelease& release);
}
