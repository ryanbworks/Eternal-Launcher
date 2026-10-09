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

// Fronteira responsável por disponibilizar um Java portátil sem alterar a
// instalação global do Windows ou exigir configuração manual do usuário.
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
