#pragma once

#include <filesystem>

// Fronteira reservada para uma política futura de atualização incremental.
// A instalação atual já valida downloads, mas não remove arquivos obsoletos.
class IUpdateService
{
public:
    virtual ~IUpdateService() = default;

    // TODO: detectar arquivos gerenciados que foram removidos do manifesto.
    virtual bool needsUpdate(const std::filesystem::path& installationPath) = 0;

    // TODO: aplicar remoções com backup/rollback antes de limpar os obsoletos.
    virtual void updateFiles(const std::filesystem::path& installationPath) = 0;
};
