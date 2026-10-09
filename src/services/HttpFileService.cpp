#include "services/HttpFileService.h"
#include "services/HttpDownloadClient.h"
#include <windows.h>
#include <chrono>

HttpFileService::HttpFileService(std::wstring baseUrl):baseUrl_(std::move(baseUrl)) {}
FileServerManifestResponse HttpFileService::fetchManifest(std::stop_token stop) {
    const auto started=std::chrono::steady_clock::now();
    auto base=baseUrl_;if(!base.ends_with(L'/')) base+=L'/';
    const auto response=HttpDownloadClient{}.getText(base+L"manifest.json",stop);
    FileServerManifestResponse result;
    result.httpStatus=response.statusCode;result.errorMessage=response.errorMessage;result.failure=response.failure;
    result.elapsedMilliseconds=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    if(response.success) {
        const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,response.content.data(),static_cast<int>(response.content.size()),nullptr,0);
        if(size>0) {
            result.content.resize(size);
            MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,response.content.data(),static_cast<int>(response.content.size()),result.content.data(),size);
            result.success=true;
        } else {result.errorMessage=L"O manifesto retornado não contém texto UTF-8 válido.";result.failure=InstallationFailure::Configuration;}
    }
    return result;
}
