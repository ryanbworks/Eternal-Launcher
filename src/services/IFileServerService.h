#pragma once

#include <string>

// Contrato genérico para o futuro servidor HTTP de arquivos. A interface não
// conhece NGINX, permitindo trocar a infraestrutura sem afetar as telas.
class IFileServerService
{
public:
    virtual ~IFileServerService() = default;

    // TODO: consultar a disponibilidade do servidor HTTP de arquivos.
    virtual bool isAvailable() = 0;

    // TODO: obter o manifesto remoto sem acoplar a aplicação ao servidor utilizado.
    virtual std::wstring fetchManifest() = 0;
};
