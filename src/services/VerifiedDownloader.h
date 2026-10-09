#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>
#include "types/InstallationProgress.h"

enum class FileHashAlgorithm
{
    Sha1,
    Sha256
};

struct VerifiedDownload
{
    std::wstring url;
    std::filesystem::path destination;
    std::string expectedHash;
    std::uint64_t expectedSize = 0;
    FileHashAlgorithm hashAlgorithm = FileHashAlgorithm::Sha1;
};

struct VerifiedDownloadResult
{
    bool success = false;
    std::wstring message;
    InstallationFailure failure = InstallationFailure::None;
    bool allAvailable = false;
};

using VerifiedDownloadProgress = InstallationProgressCallback;

// Executa downloads paralelos com arquivos .part e só publica cada destino
// depois que tamanho e hash coincidirem com o manifesto de origem.
class VerifiedDownloader
{
public:
    [[nodiscard]] VerifiedDownloadResult downloadAll(
        const std::vector<VerifiedDownload>& files,
        std::stop_token stopToken,
        const VerifiedDownloadProgress& progress,
        unsigned workerCount = 8) const;
};
