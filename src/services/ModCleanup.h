#pragma once

#include <filesystem>
#include <stop_token>
#include <vector>

inline constexpr wchar_t ManagedModsRecordName[] = L".eternal-managed-mods";

void removeObsoleteMods(const std::filesystem::path& modsDirectory,
                        const std::vector<std::filesystem::path>& expectedFiles,std::stop_token stop = {});
