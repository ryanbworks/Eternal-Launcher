#pragma once

#include <string>

struct FileServerManifestResponse
{
    bool success = false;
    unsigned long httpStatus = 0;
    std::wstring content;
    std::wstring errorMessage;
    long long elapsedMilliseconds = 0;
};

// Contrato genérico para obtenção do manifesto no servidor HTTP de arquivos.
// Nenhuma referência ao NGINX aparece aqui, pois ele é apenas a hospedagem atual.
class IFileServerService
{
public:
    virtual ~IFileServerService() = default;

    // Obtém o manifesto e devolve dados suficientes para a interface representar
    // sucesso, indisponibilidade ou respostas HTTP inesperadas.
    virtual FileServerManifestResponse fetchManifest() = 0;
};
