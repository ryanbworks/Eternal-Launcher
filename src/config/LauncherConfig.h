#pragma once

#include <string_view>

namespace LauncherConfig
{
    // Versões alvo exibidas pela interface. Futuramente estes valores poderão
    // vir de um manifesto remoto mantido no servidor HTTP de arquivos.
    inline constexpr std::wstring_view MinecraftVersion = L"1.20.1";
    inline constexpr std::wstring_view ForgeVersion = L"47.4.10";
    inline constexpr std::wstring_view VersionProfile = L"1.20.1-forge-47.4.10";
    inline constexpr std::wstring_view AssetIndex = L"5";
    inline constexpr std::wstring_view LauncherVersion = L"0.1.0-visual";
    // MOCK: nome inicial apenas para tornar o campo visível durante a validação do design.
    inline constexpr std::wstring_view DefaultNickname = L"Steve";

    // Endereço atual do serviço de arquivos. Ele permanece genérico para que a
    // aplicação não dependa de detalhes do NGINX utilizado no servidor.
    // TODO: carregar a URL base de uma configuração externa por ambiente.
    inline constexpr std::wstring_view FileServerBaseUrl = L"http://10.0.0.51";
    inline constexpr std::wstring_view FileServerLabel = L"Servidor HTTP";

    // A instância isolada evita misturar os mods do Eternal Launcher com outras
    // instalações existentes na pasta .minecraft do usuário.
    inline constexpr std::wstring_view DataDirectoryName = L".eternal-launcher";
    inline constexpr std::wstring_view GameDirectoryName = L"minecraft";
    inline constexpr int MinimumMemoryMegabytes = 512;
    inline constexpr int MaximumMemoryMegabytes = 4096;
}
