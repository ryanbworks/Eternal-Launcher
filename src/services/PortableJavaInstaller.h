#pragma once

#include <filesystem>
#include <stop_token>
#include <string>

struct PortableJavaInstallResult
{
    bool success = false;
    std::wstring message;
};

// Extracts and activates an already validated Java package. Swapping by rename keeps
// the previous runtime if preparing the new package fails.
class PortableJavaInstaller
{
public:
    [[nodiscard]] static bool isJava17Runtime(const std::filesystem::path& runtime);
    [[nodiscard]] static PortableJavaInstallResult install(
        const std::filesystem::path& archive,
        const std::filesystem::path& dataDirectory,
        std::stop_token stopToken);
};
