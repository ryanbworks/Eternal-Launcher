#pragma once

#include "services/IFileServerService.h"

#include <string>

// HTTP implementation of the file service based on Windows WinHTTP.
// The class knows the protocol but doesn't depend on the server software.
class HttpFileService final : public IFileServerService
{
public:
    explicit HttpFileService(std::wstring baseUrl);

    FileServerManifestResponse fetchManifest(std::stop_token stop = {}) override;

private:
    std::wstring baseUrl_;
};
