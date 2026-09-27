#pragma once

#include <filesystem>

// Fronteira reservada para uma política futura de atualização incremental.
// A instalação atual já valida downloads, mas não remove arquivos obsoletos.
class IUpdateService
{
public:
    virtual ~IUpdateService() = default;

    // TODO: comparar hashes locais com o manifesto recebido pelo servidor HTTP.
    virtual bool needsUpdate(const std::filesystem::path& installationPath) = 0;

    // TODO: baixar apenas os arquivos ausentes, diferentes ou desatualizados.
    virtual void updateFiles(const std::filesystem::path& installationPath) = 0;
};
