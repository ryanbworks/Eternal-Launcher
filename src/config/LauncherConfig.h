#pragma once

#include <string_view>

namespace LauncherConfig
{
    // Versões alvo exibidas pela interface. Futuramente estes valores poderão
    // vir de um manifesto remoto mantido no servidor HTTP de arquivos.
    inline constexpr std::wstring_view MinecraftVersion = L"1.20.1";
    inline constexpr std::wstring_view ForgeVersion = L"47.4.10";
    inline constexpr std::wstring_view LauncherVersion = L"0.1.0-visual";
    // MOCK: nome inicial apenas para tornar o campo visível durante a validação do design.
    inline constexpr std::wstring_view DefaultNickname = L"Steve";

    // TODO: carregar a URL base de uma configuração externa por ambiente.
    inline constexpr std::wstring_view FileServerLabel = L"Servidor HTTP";
}
