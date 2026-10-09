#pragma once

#include <string>
#include <stop_token>
#include "types/InstallationProgress.h"

struct FileServerManifestResponse
{
    bool success = false;
    unsigned long httpStatus = 0;
    std::wstring content;
    std::wstring errorMessage;
    long long elapsedMilliseconds = 0;
    InstallationFailure failure = InstallationFailure::None;
};

// Contrato genérico para obtenção do manifesto no servidor HTTP de arquivos.
// Nenhuma referência ao NGINX aparece aqui, pois ele é apenas a hospedagem atual.
class IFileServerService
{
public:
    virtual ~IFileServerService() = default;

    // Obtém o manifesto e devolve dados suficientes para a interface representar
    // sucesso, indisponibilidade ou respostas HTTP inesperadas.
    virtual FileServerManifestResponse fetchManifest(std::stop_token stop = {}) = 0;
};
