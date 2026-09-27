#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

struct HttpDownloadResult
{
    bool success = false;
    unsigned long statusCode = 0;
    std::string content;
    std::wstring errorMessage;
};

using HttpDownloadProgressCallback =
    std::function<void(std::uint64_t received, std::uint64_t total)>;

// Cliente HTTP pequeno e genérico usado para obter metadados e arquivos
// binários. Ele não conhece Java, Minecraft nem o servidor NGINX do projeto.
class HttpDownloadClient
{
public:
    [[nodiscard]] HttpDownloadResult getText(std::wstring_view url) const;
    [[nodiscard]] HttpDownloadResult downloadFile(
        std::wstring_view url,
        const std::filesystem::path& destination,
        std::stop_token stopToken,
        const HttpDownloadProgressCallback& progress) const;
};
