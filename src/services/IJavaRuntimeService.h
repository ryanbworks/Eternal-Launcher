#pragma once

#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

struct JavaRuntimeProgress
{
    int percent = 0;
    std::wstring message;
};

struct JavaRuntimeResult
{
    bool success = false;
    bool installedNow = false;
    std::wstring message;
};

using JavaRuntimeProgressCallback =
    std::function<void(int percent, std::wstring_view message)>;

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
