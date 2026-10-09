#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

struct LauncherRelease
{
    std::string version;
    std::string codename;
    std::string path;
    std::string sha256;
    std::uint64_t size = 0;
};

namespace LauncherUpdateManifest
{
    inline constexpr std::size_t MaximumManifestSize = 64 * 1024;
    inline constexpr std::uint64_t MaximumExecutableSize = 256ULL * 1024 * 1024;
    using Version = std::array<unsigned, 3>;
    std::optional<Version> parseVersion(std::string_view text);
    // Test fixtures can supply their own public key; production always uses the pinned key.
    std::optional<LauncherRelease> verify(std::string_view envelope, std::wstring& error,
        std::span<const unsigned char> publicKey = {}, std::string_view architecture = {});
}
