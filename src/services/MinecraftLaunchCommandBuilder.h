#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include "types/MinecraftIdentity.h"

// Monta a linha de comando específica do Minecraft 1.20.1 com Forge 47.4.10.
// Separar esta responsabilidade mantém o serviço de processo focado em validar
// a instalação, iniciar o Java e acompanhar o jogo.
class MinecraftLaunchCommandBuilder
{
public:
    [[nodiscard]] std::wstring build(
        std::wstring_view nickname,
        const std::filesystem::path& javaExecutable,
        const std::filesystem::path& gameDirectory) const;
    [[nodiscard]] std::wstring build(
        const MinecraftIdentity& identity,
        const std::filesystem::path& javaExecutable,
        const std::filesystem::path& gameDirectory) const;

private:
    [[nodiscard]] std::wstring buildClasspath(
        const std::filesystem::path& gameDirectory) const;
};
