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
    inline constexpr std::wstring_view LauncherVersion = L"0.1.0-Muriçoca";
    // Endereço atual do serviço de arquivos. Ele permanece genérico para que a
    // aplicação não dependa de detalhes do NGINX utilizado no servidor.
    // TODO: carregar a URL base de uma configuração externa por ambiente.
    inline constexpr std::wstring_view FileServerBaseUrl = L"http://10.0.0.51";
    inline constexpr std::wstring_view FileServerLabel = L"Servidor HTTP";

    // A API oficial do Adoptium informa o ZIP portátil mais recente do Temurin
    // 17 e seu SHA-256. O runtime é baixado diretamente da origem informada.
    inline constexpr std::wstring_view JavaRuntimeMetadataUrl =
        L"https://api.adoptium.net/v3/assets/latest/17/hotspot"
        L"?architecture=x64&image_type=jre&os=windows&vendor=eclipse";
    inline constexpr std::wstring_view ForgeInstallerUrl =
        L"https://maven.minecraftforge.net/net/minecraftforge/forge/1.20.1-47.4.10/"
        L"forge-1.20.1-47.4.10-installer.jar";
    inline constexpr std::string_view ForgeInstallerSha1 =
        "66bfea9963bfa60d88bab6b2750e74a958392715";

    // A instância isolada evita misturar os mods do Eternal Launcher com outras
    // instalações existentes na pasta .minecraft do usuário.
    inline constexpr std::wstring_view DataDirectoryName = L".eternal-launcher";
    inline constexpr std::wstring_view GameDirectoryName = L"minecraft";
    inline constexpr int MinimumMemoryMegabytes = 512;
    inline constexpr int MaximumMemoryMegabytes = 4096;
}
