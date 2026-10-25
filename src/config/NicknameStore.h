#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// Persists only the local nickname; it is not authentication or a Minecraft account.
class NicknameStore
{
public:
    [[nodiscard]] static bool isValid(std::wstring_view nickname);
    [[nodiscard]] static std::wstring load(const std::filesystem::path& dataDirectory);
    [[nodiscard]] static bool save(
        const std::filesystem::path& dataDirectory,
        std::wstring_view nickname);
    [[nodiscard]] static std::filesystem::path filePath(
        const std::filesystem::path& dataDirectory);
};
