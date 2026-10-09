#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

struct LauncherUpdateResponse { bool success = false; std::string text; std::wstring error; };
using LauncherUpdateDownloadProgress = std::function<void(std::uint64_t)>;
class ILauncherUpdateTransport
{
public:
    virtual ~ILauncherUpdateTransport() = default;
    virtual LauncherUpdateResponse manifest(std::stop_token stop) const = 0;
    virtual LauncherUpdateResponse download(std::string_view path, const std::filesystem::path& destination,
        std::uint64_t size, std::stop_token stop, const LauncherUpdateDownloadProgress& progress) const = 0;
};
class LauncherUpdateTransport : public ILauncherUpdateTransport
{
public:
    LauncherUpdateResponse manifest(std::stop_token stop) const override;
    LauncherUpdateResponse download(std::string_view path, const std::filesystem::path& destination,
        std::uint64_t size, std::stop_token stop, const LauncherUpdateDownloadProgress& progress) const override;
};
