#pragma once

#include <cstdint>
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
    // Conta Eternal: the group's account and skin server (Drasl). Overridable with
    // ETERNAL_ACCOUNTS_URL for local tests.
    inline constexpr std::wstring_view AccountsBaseUrl = L"https://accountseternal.aplot.cloud";
    // authlib-injector points the game at the account server. Pinned version and hash.
    inline constexpr std::wstring_view AuthlibInjectorUrl =
        L"https://authlib-injector.yushi.moe/artifact/56/authlib-injector-1.2.8.jar";
    inline constexpr std::wstring_view AuthlibInjectorMirrorUrl =
        L"https://github.com/yushijinhun/authlib-injector/releases/download/v1.2.8/authlib-injector-1.2.8.jar";
    inline constexpr std::string_view AuthlibInjectorSha256 =
        "9c7f4343e6c82034958ffb48c14a2cb0c85928be7283103ce17da00c6d5a7b10";
    inline constexpr std::wstring_view AuthlibInjectorFileName = L"authlib-injector-1.2.8.jar";

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

    // Unmodified 1.8.9, kept apart from the group's instance.
    inline constexpr std::wstring_view VanillaVersion = L"1.8.9";
    inline constexpr std::wstring_view VanillaAssetIndex = L"1.8";
    inline constexpr std::wstring_view VanillaGameDirectoryName = L"minecraft-1.8.9";
    // OptiFine for the 1.8.9. Its license forbids redistributing the file, so each
    // launcher fetches it from optifine.net: the download page hands out a link with a
    // token, and the file must match the pinned size and SHA-256.
    inline constexpr std::wstring_view OptiFineFileName = L"OptiFine_1.8.9_HD_U_M5.jar";
    inline constexpr std::wstring_view OptiFinePageUrl = L"https://optifine.net/adloadx?f=OptiFine_1.8.9_HD_U_M5.jar";
    inline constexpr std::string_view OptiFineSha256 = "2739760a70fca6ae5c7dc817bcc6283f18f55e38fa23778da7f387a6b724b2c4";
    inline constexpr std::uint64_t OptiFineSize = 2585014;
    // OptiFine ships its own launchwrapper inside the jar.
    inline constexpr std::wstring_view OptiFineLaunchwrapper = L"launchwrapper-of-2.2.jar";
    inline constexpr std::string_view OptiFineLaunchwrapperSha256 = "09d8f19ac2fb654809f72469106bcbabbde178db28903c4abfcf4a9114cc3a8c";
    inline constexpr std::wstring_view OptiFineVersionName = L"1.8.9-OptiFine_HD_U_M5";
    inline constexpr std::wstring_view Java8RuntimeDirectoryName = L"runtime-8";
    inline constexpr std::wstring_view Java8RuntimeMetadataUrl =
        L"https://api.adoptium.net/v3/assets/latest/8/hotspot"
        L"?architecture=x64&image_type=jre&os=windows&vendor=eclipse";
    inline constexpr int MinimumMemoryMegabytes = 512;
    inline constexpr int MaximumMemoryMegabytes = 4096;
}
