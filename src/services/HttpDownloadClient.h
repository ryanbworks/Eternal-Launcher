#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include "types/InstallationProgress.h"

struct HttpDownloadResult
{
    bool success = false;
    unsigned long statusCode = 0;
    std::string content;
    std::wstring errorMessage;
    InstallationFailure failure = InstallationFailure::None;
    bool cancelled = false;
};

using HttpDownloadProgressCallback =
    std::function<void(std::uint64_t received, std::uint64_t total)>;

// Small, generic HTTP client used to fetch metadata and binary
// files. It knows nothing about Java, Minecraft or the project's NGINX server.
class HttpDownloadClient
{
public:
    [[nodiscard]] HttpDownloadResult getText(std::wstring_view url, std::stop_token stopToken = {}) const;
    // Each network step waits at most `waitMilliseconds`, so a caller can bound a best-effort call.
    [[nodiscard]] HttpDownloadResult postJson(std::wstring_view url, const std::string& body,
        std::stop_token stopToken = {}, unsigned long waitMilliseconds = 15000) const;
    // Uploads raw bytes (application/octet-stream), waiting up to 30 s per step.
    [[nodiscard]] HttpDownloadResult postBinary(std::wstring_view url, const std::string& body,
        std::stop_token stopToken = {}) const;
    [[nodiscard]] HttpDownloadResult downloadFile(
        std::wstring_view url,
        const std::filesystem::path& destination,
        std::stop_token stopToken,
        const HttpDownloadProgressCallback& progress) const;
};
