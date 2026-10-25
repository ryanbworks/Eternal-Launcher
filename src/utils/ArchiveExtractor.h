#pragma once

#include <filesystem>
#include <stop_token>

namespace ArchiveExtractor
{
    // Uses the bsdtar shipped with Windows for ZIP/JAR without depending on an
    // interpreter or a tool installed by the user.
    [[nodiscard]] bool extract(
        const std::filesystem::path& archive,
        const std::filesystem::path& destination,
        std::stop_token stopToken);
}
