#include "services/VerifiedDownloader.h"

#include "services/HttpDownloadClient.h"
#include "utils/FileHash.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace
{
    std::string hashFile(const VerifiedDownload& file, const std::filesystem::path& path)
    {
        return file.hashAlgorithm == FileHashAlgorithm::Sha256
            ? FileHash::sha256(path)
            : FileHash::sha1(path);
    }

    bool isCurrent(const VerifiedDownload& file)
    {
        if (!std::filesystem::is_regular_file(file.destination)) return false;
        if (file.expectedSize > 0 &&
            std::filesystem::file_size(file.destination) != file.expectedSize) return false;
        return !file.expectedHash.empty() && hashFile(file, file.destination) == file.expectedHash;
    }
}

VerifiedDownloadResult VerifiedDownloader::downloadAll(
    const std::vector<VerifiedDownload>& files,
    std::stop_token stopToken,
    const VerifiedDownloadProgress& progress,
    unsigned workerCount) const
{
    if (files.empty()) return {true, L"Nenhum arquivo precisa ser baixado."};

    std::uint64_t totalBytes = 0;
    for (const auto& file : files) totalBytes += file.expectedSize;

    std::atomic_size_t nextIndex = 0;
    std::atomic_uint64_t completedBytes = 0;
    std::atomic_int lastReportedPercent = -1;
    std::atomic_bool failed = false;
    std::mutex errorMutex;
    std::wstring errorMessage;

    const auto publishProgress = [&]
    {
        if (!progress) return;
        const auto completed = completedBytes.load();
        const int percent = totalBytes > 0
            ? static_cast<int>(std::min<std::uint64_t>(100, completed * 100 / totalBytes))
            : 100;
        int previous = lastReportedPercent.load();
        if (percent != previous && lastReportedPercent.compare_exchange_strong(previous, percent))
        {
            progress(completed, totalBytes);
        }
    };

    const auto worker = [&]
    {
        HttpDownloadClient http;
        while (!failed.load() && !stopToken.stop_requested())
        {
            const auto index = nextIndex.fetch_add(1);
            if (index >= files.size()) return;
            const auto& file = files[index];

            if (isCurrent(file))
            {
                completedBytes.fetch_add(file.expectedSize);
                publishProgress();
                continue;
            }

            auto partial = file.destination;
            partial += L".part";
            std::uint64_t receivedForFile = 0;
            const auto result = http.downloadFile(
                file.url, partial, stopToken,
                [&](std::uint64_t received, std::uint64_t)
                {
                    const auto delta = received - receivedForFile;
                    receivedForFile = received;
                    completedBytes.fetch_add(delta);
                    publishProgress();
                });

            const bool unexpectedSize = file.expectedSize > 0 &&
                (!std::filesystem::is_regular_file(partial) ||
                 std::filesystem::file_size(partial) != file.expectedSize);
            if (!result.success || unexpectedSize ||
                hashFile(file, partial) != file.expectedHash)
            {
                std::error_code ignored;
                std::filesystem::remove(partial, ignored);
                failed = true;
                std::scoped_lock lock(errorMutex);
                errorMessage = result.success
                    ? L"A integridade de um arquivo baixado não confere: " +
                          file.destination.wstring()
                    : result.errorMessage;
                return;
            }

            // MoveFileEx substitui o destino somente depois da verificação e
            // evita a janela em que a instalação ficaria sem o arquivo antigo.
            if (!MoveFileEx(
                    partial.c_str(), file.destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                std::error_code ignored;
                std::filesystem::remove(partial, ignored);
                failed = true;
                std::scoped_lock lock(errorMutex);
                errorMessage = L"Não foi possível publicar o arquivo verificado: " +
                               file.destination.wstring();
                return;
            }
        }
    };

    std::vector<std::jthread> workers;
    const auto actualWorkers = std::min<unsigned>(
        std::max(1U, workerCount), static_cast<unsigned>(files.size()));
    workers.reserve(actualWorkers);
    for (unsigned index = 0; index < actualWorkers; ++index)
    {
        workers.emplace_back(worker);
    }
    workers.clear(); // O destrutor de jthread aguarda todos os workers.

    if (stopToken.stop_requested()) return {false, L"Instalação cancelada."};
    if (failed.load()) return {false, errorMessage};
    publishProgress();
    return {true, L"Arquivos baixados e verificados."};
}
