#pragma once

#include <filesystem>
#include <stop_token>

namespace ArchiveExtractor
{
    // Usa o bsdtar fornecido pelo Windows para ZIP/JAR sem depender de um
    // interpretador ou ferramenta instalada pelo usuário.
    [[nodiscard]] bool extract(
        const std::filesystem::path& archive,
        const std::filesystem::path& destination,
        std::stop_token stopToken);
}
