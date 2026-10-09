#pragma once

#include <filesystem>
#include <stop_token>
#include <vector>

// Remove apenas JARs diretos da pasta de mods que não constam no manifesto.
// Chamar somente depois de validar o manifesto e concluir os downloads.
void removeObsoleteMods(const std::filesystem::path& modsDirectory,
                        const std::vector<std::filesystem::path>& expectedFiles,std::stop_token stop = {});
