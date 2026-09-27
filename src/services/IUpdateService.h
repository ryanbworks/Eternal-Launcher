#pragma once

#include <filesystem>

// Define as responsabilidades futuras de comparação e atualização de arquivos.
// Nenhuma implementação concreta é criada durante a etapa exclusivamente visual.
class IUpdateService
{
public:
    virtual ~IUpdateService() = default;

    // TODO: comparar hashes locais com o manifesto recebido pelo servidor HTTP.
    virtual bool needsUpdate(const std::filesystem::path& installationPath) = 0;

    // TODO: baixar apenas os arquivos ausentes, diferentes ou desatualizados.
    virtual void updateFiles(const std::filesystem::path& installationPath) = 0;
};
