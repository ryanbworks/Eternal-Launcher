#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <stop_token>

// Represents the ZIP package reported by the Adoptium API and centralizes
// metadata parsing and the file's cryptographic verification.
struct JavaRuntimePackage
{
    std::wstring url;
    std::string sha256;
    std::uint64_t size = 0;

    [[nodiscard]] static std::optional<JavaRuntimePackage> parse(
        std::string_view metadata);
    [[nodiscard]] bool matchesFile(const std::filesystem::path& file, std::stop_token stop = {}) const;
};
