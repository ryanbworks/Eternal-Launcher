#pragma once

#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include "types/InstallationProgress.h"

using JavaRuntimeProgress = InstallationProgress;

struct JavaRuntimeResult
{
    bool success = false;
    bool installedNow = false;
    std::wstring message;
    InstallationFailure failure = InstallationFailure::None;
    InstallationOutcome outcome = InstallationOutcome::Failed;
};

using JavaRuntimeProgressCallback = InstallationProgressCallback;

// Boundary responsible for providing a portable Java without changing the
// global Windows installation or requiring manual setup from the user.
class IJavaRuntimeService
{
public:
    virtual ~IJavaRuntimeService() = default;

    [[nodiscard]] virtual bool isInstalled() const = 0;
    [[nodiscard]] virtual std::filesystem::path runtimeDirectory() const = 0;
    virtual JavaRuntimeResult ensureInstalled(
        std::stop_token stopToken,
        const JavaRuntimeProgressCallback& progress) = 0;
};
