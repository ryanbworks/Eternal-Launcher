#pragma once

#include "services/IJavaRuntimeService.h"

// Baixa o Temurin 17 portátil, confere o SHA-256 publicado pelo Adoptium e
// instala o conteúdo de forma isolada e transacional para o Eternal Launcher.
class JavaRuntimeService final : public IJavaRuntimeService
{
public:
    [[nodiscard]] bool isInstalled() const override;
    [[nodiscard]] std::filesystem::path runtimeDirectory() const override;
    JavaRuntimeResult ensureInstalled(
        std::stop_token stopToken,
        const JavaRuntimeProgressCallback& progress) override;

private:
    [[nodiscard]] std::filesystem::path dataDirectory() const;
};
