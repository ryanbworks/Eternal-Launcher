#pragma once

#include <filesystem>
#include <string>
#include <stop_token>

namespace FileHash
{
    // Hashes são usados para aceitar somente arquivos idênticos aos manifestos
    // publicados pela Mojang, Forge, Adoptium ou servidor privado.
    [[nodiscard]] std::string sha1(const std::filesystem::path& file, std::stop_token stop = {});
    [[nodiscard]] std::string sha256(const std::filesystem::path& file, std::stop_token stop = {});
}
