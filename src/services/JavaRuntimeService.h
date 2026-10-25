#pragma once

#include "services/IJavaRuntimeService.h"

// Downloads portable Temurin 17, checks the SHA-256 published by Adoptium and
// installs the content in an isolated, transactional way for Eternal Launcher.
class JavaRuntimeService final : public IJavaRuntimeService
{
public:
    [[nodiscard]] bool isInstalled() const override;
    [[nodiscard]] std::filesystem::path runtimeDirectory() const override;
    JavaRuntimeResult ensureInstalled(
        std::stop_token stopToken,
        const JavaRuntimeProgressCallback& progress) override;
    JavaRuntimeResult reinstall(
        std::stop_token stopToken,
        const JavaRuntimeProgressCallback& progress);

private:
    JavaRuntimeResult install(
        std::stop_token stopToken,
        const JavaRuntimeProgressCallback& progress,
        bool force);
    [[nodiscard]] std::filesystem::path dataDirectory() const;
};
