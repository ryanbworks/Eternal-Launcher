#pragma once

#include <filesystem>
#include <string>
#include <stop_token>

namespace FileHash
{
    // Hashes are used to accept only files identical to the manifests
    // published by Mojang, Forge, Adoptium or the private server.
    [[nodiscard]] std::string sha1(const std::filesystem::path& file, std::stop_token stop = {});
    [[nodiscard]] std::string sha256(const std::filesystem::path& file, std::stop_token stop = {});
}
