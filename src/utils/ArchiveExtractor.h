#pragma once

#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace ArchiveExtractor
{
    // Uses the bsdtar shipped with Windows for ZIP/JAR without depending on an
    // interpreter or a tool installed by the user.
    [[nodiscard]] bool extract(
        const std::filesystem::path& archive,
        const std::filesystem::path& destination,
        std::stop_token stopToken);
    // Extracts only `entry` (a path inside the archive) into `destination`.
    [[nodiscard]] bool extractEntry(
        const std::filesystem::path& archive,
        const std::wstring& entry,
        const std::filesystem::path& destination,
        std::stop_token stopToken);

    // Extracts the native JARs into `directory`, unless it already holds them: a marker
    // remembers which archives (path, size, write time) produced the folder, and
    // `required` (a file name in the folder) must still be there. `reused` says which.
    [[nodiscard]] bool prepareNatives(
        const std::vector<std::filesystem::path>& archives,
        const std::filesystem::path& directory,
        const std::filesystem::path& required,
        std::stop_token stopToken,
        bool& reused);
    // Records that `directory` now holds what `archives` produce.
    void rememberNatives(const std::vector<std::filesystem::path>& archives, const std::filesystem::path& directory);
}
