#include "services/VerifiedDownloader.h"
#include "services/HttpDownloadClient.h"
#include "utils/FileHash.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace {
    std::string hashFile(const VerifiedDownload& file,const std::filesystem::path& path,std::stop_token stop) {
        return file.hashAlgorithm==FileHashAlgorithm::Sha256 ? FileHash::sha256(path,stop) : FileHash::sha1(path,stop);
    }
    bool isCurrent(const VerifiedDownload& file,std::stop_token stop) {
        return std::filesystem::is_regular_file(file.destination) &&
            (!file.expectedSize || std::filesystem::file_size(file.destination)==file.expectedSize) &&
            !file.expectedHash.empty() && hashFile(file,file.destination,stop)==file.expectedHash;
    }
    struct PartialCleanup {
        std::filesystem::path path;
        ~PartialCleanup() { std::error_code ignored;std::filesystem::remove(path,ignored); }
    };
}

VerifiedDownloadResult VerifiedDownloader::downloadAll(const std::vector<VerifiedDownload>& files,
    std::stop_token stop,const VerifiedDownloadProgress& callback,unsigned workerCount) const {
    InstallationProgress progress;
    progress.activity=InstallationActivity::Checking;
    progress.message=L"Verificando arquivos existentes...";
    progress.totalFiles=files.size();
    if(callback) callback(progress);
    std::vector<const VerifiedDownload*> queue;
    try {
        for(const auto& file:files) {
            if(stop.stop_requested()) return {false,L"Instalação cancelada."};
            if(isCurrent(file,stop)) ++progress.reusedFiles;
            else queue.push_back(&file);
        }
    } catch(const std::exception&) {return {false,L"Não foi possível conferir os arquivos. Confira permissões da pasta.",InstallationFailure::Disk};}
    if(stop.stop_requested()) return {false,L"Instalação cancelada."};
    progress.activity=queue.empty() ? InstallationActivity::Verifying : InstallationActivity::Downloading;
    progress.totalFiles=queue.size();
    progress.message=queue.empty() ? L"Arquivos já disponíveis e verificados." : L"Baixando arquivos...";
    std::vector<std::uint64_t> sizes;
    std::uint64_t total=0;
    bool known=true;
    for(const auto* file:queue) {sizes.push_back(file->expectedSize);total+=file->expectedSize;known &= file->expectedSize>0;}
    if(known) progress.totalBytes=total;
    if(!queue.empty() && known && total) progress.percent=0;
    if(callback) callback(progress);
    if(queue.empty()) return {true,L"Arquivos já disponíveis e verificados.",InstallationFailure::None,true};

    std::stop_source workersStop;
    std::stop_callback parentStop(stop,[&]{workersStop.request_stop();});
    const auto token=workersStop.get_token();
    std::atomic_size_t nextIndex=0;
    std::mutex mutex;
    VerifiedDownloadResult result{true,L"Arquivos baixados e verificados."};
    std::size_t transfersFinished=0;
    const auto publish=[&] { // Call under mutex; callbacks are serialized across all workers.
        if(progress.totalBytes && *progress.totalBytes) progress.percent=std::min(99,
            static_cast<int>(static_cast<double>(progress.receivedBytes)*100/static_cast<double>(*progress.totalBytes)));
        if(callback) callback(progress);
    };
    const auto failure=[&](InstallationFailure category,std::wstring message) {
        std::scoped_lock lock(mutex);
        if(result.success) { result={false,std::move(message),category};workersStop.request_stop(); }
    };
    const auto worker=[&] {
        HttpDownloadClient http;
        while(!token.stop_requested()) {
            const auto index=nextIndex.fetch_add(1);
            if(index>=queue.size()) return;
            const auto& file=*queue[index];
            auto partial=file.destination;partial+=L".part";
            PartialCleanup cleanup{partial};
            try {
                std::uint64_t previous=0;
                const auto response=http.downloadFile(file.url,partial,token,
                    [&](std::uint64_t received,std::uint64_t size) {
                        std::scoped_lock lock(mutex);
                        if(!sizes[index] && size) {
                            sizes[index]=size;
                            if(std::all_of(sizes.begin(),sizes.end(),[](auto s){return s>0;})) {
                                std::uint64_t sum=0;for(auto s:sizes) sum+=s;progress.totalBytes=sum;
                            }
                        }
                        progress.receivedBytes+=received-previous;previous=received;publish();
                    });
                if(!response.success) {
                    if(!token.stop_requested()) failure(response.failure,response.errorMessage);
                    return;
                }
                {
                    std::scoped_lock lock(mutex);
                    ++transfersFinished;
                    if(transfersFinished==queue.size()) {
                        progress.activity=InstallationActivity::Verifying;progress.message=L"Verificando integridade dos arquivos...";
                        publish();
                    }
                }
                if(token.stop_requested()) return;
                const bool wrongSize=file.expectedSize && std::filesystem::file_size(partial)!=file.expectedSize;
                const auto hash=wrongSize ? std::string{} : hashFile(file,partial,token);
                if(token.stop_requested()) return;
                if(wrongSize || hash.empty() || hash!=file.expectedHash) {
                    failure(InstallationFailure::Integrity,L"A integridade de um arquivo não confere: "+file.destination.filename().wstring()+L". Tente novamente.");return;
                }
                if(token.stop_requested()) return;
                if(!MoveFileExW(partial.c_str(),file.destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
                    failure(InstallationFailure::Disk,L"Não foi possível publicar o arquivo. Confira espaço livre e permissões.");return;
                }
                {std::scoped_lock lock(mutex);++progress.completedFiles;publish();}
            } catch(const std::exception&) {
                failure(InstallationFailure::Disk,L"Não foi possível preparar os arquivos. Confira espaço livre e permissões.");return;
            }
        }
    };
    std::vector<std::jthread> workers;
    const auto count=std::min<std::size_t>(std::max(1U,workerCount),queue.size());
    for(std::size_t i=0;i<count;++i) workers.emplace_back(worker);
    for(auto& thread:workers) thread.join();
    if(stop.stop_requested()) return {false,L"Instalação cancelada."};
    if(result.success) {progress.percent=progress.totalBytes ? std::optional<int>(100) : std::nullopt;if(callback) callback(progress);}
    return result;
}
