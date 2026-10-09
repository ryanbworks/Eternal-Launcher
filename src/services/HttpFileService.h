#pragma once

#include "services/IFileServerService.h"

#include <string>

// Implementação HTTP do serviço de arquivos baseada no WinHTTP do Windows.
// A classe conhece o protocolo, mas não depende do software usado no servidor.
class HttpFileService final : public IFileServerService
{
public:
    explicit HttpFileService(std::wstring baseUrl);

    FileServerManifestResponse fetchManifest(std::stop_token stop = {}) override;

private:
    std::wstring baseUrl_;
};
