#pragma once

#include <filesystem>
#include <stop_token>
#include <string>

struct PortableJavaInstallResult
{
    bool success = false;
    std::wstring message;
};

// Extrai e ativa um pacote Java já validado. A troca por rename preserva o
// runtime anterior caso a preparação do novo pacote falhe.
class PortableJavaInstaller
{
public:
    [[nodiscard]] static bool isJava17Runtime(const std::filesystem::path& runtime);
    [[nodiscard]] static PortableJavaInstallResult install(
        const std::filesystem::path& archive,
        const std::filesystem::path& dataDirectory,
        std::stop_token stopToken);
};
