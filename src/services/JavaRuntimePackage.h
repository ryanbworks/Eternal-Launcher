#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// Representa o pacote ZIP informado pela API do Adoptium e concentra a
// interpretação dos metadados e a verificação criptográfica do arquivo.
struct JavaRuntimePackage
{
    std::wstring url;
    std::string sha256;
    std::uint64_t size = 0;

    [[nodiscard]] static std::optional<JavaRuntimePackage> parse(
        std::string_view metadata);
    [[nodiscard]] bool matchesFile(const std::filesystem::path& file) const;
};
