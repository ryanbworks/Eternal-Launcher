#pragma once

#include <string_view>
#include "config/LauncherVersion.h"

namespace LauncherConfig
{
    // Target versions shown by the interface. In the future these values may
    // come from a remote manifest kept on the HTTP file server.
    inline constexpr std::wstring_view MinecraftVersion = L"1.20.1";
    inline constexpr std::wstring_view ForgeVersion = L"47.4.10";
    inline constexpr std::wstring_view VersionProfile = L"1.20.1-forge-47.4.10";
    inline constexpr std::wstring_view AssetIndex = L"5";
    inline constexpr std::wstring_view LauncherVersion = ETERNAL_VERSION_DISPLAY;
    inline constexpr std::string_view NumericVersion = ETERNAL_VERSION_NUMERIC;
    inline constexpr std::wstring_view UpdateManifestUrl = L"https://eternalfast.aplot.cloud/launcher/latest.json";
    inline constexpr std::wstring_view LauncherDownloadUrl = L"https://eternalfast.aplot.cloud/launcher/";
    // Public Microsoft application ID. It can be overridden with the
    // ETERNAL_LAUNCHER_MICROSOFT_CLIENT_ID variable without changing the executable.
    inline constexpr std::wstring_view MicrosoftClientId =
        L"0e262947-bda1-46dc-b648-e1dae22f854e";
    // Current address of the file service. It stays generic so the
    // application never depends on details of the NGINX used on the server.
    // TODO: load the base URL from an external per-environment configuration.
    inline constexpr std::wstring_view FileServerBaseUrl = L"https://eternalfast.aplot.cloud";
    inline constexpr std::wstring_view FileServerLabel = L"Servidor HTTP";

    // The official Adoptium API reports the latest portable Temurin 17 ZIP
    // and its SHA-256. The runtime is downloaded straight from the reported source.
    inline constexpr std::wstring_view JavaRuntimeMetadataUrl =
        L"https://api.adoptium.net/v3/assets/latest/17/hotspot"
        L"?architecture=x64&image_type=jre&os=windows&vendor=eclipse";
    inline constexpr std::wstring_view ForgeInstallerUrl =
        L"https://maven.minecraftforge.net/net/minecraftforge/forge/1.20.1-47.4.10/"
        L"forge-1.20.1-47.4.10-installer.jar";
    inline constexpr std::string_view ForgeInstallerSha1 =
        "66bfea9963bfa60d88bab6b2750e74a958392715";

    // The isolated instance keeps Eternal Launcher's mods from mixing with other
    // installations in the user's .minecraft folder.
    inline constexpr std::wstring_view DataDirectoryName = L".eternal-launcher";
    inline constexpr std::wstring_view GameDirectoryName = L"minecraft";
    inline constexpr int MinimumMemoryMegabytes = 512;
    inline constexpr int MaximumMemoryMegabytes = 4096;
}
