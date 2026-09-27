#include "services/MinecraftManifestService.h"

#include "config/LauncherConfig.h"
#include "services/HttpDownloadClient.h"
#include "utils/JsonValue.h"

#include <windows.h>

#include <cstdint>
#include <optional>

namespace
{
    constexpr std::wstring_view VersionManifestUrl =
        L"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
    constexpr std::wstring_view AssetObjectBaseUrl =
        L"https://resources.download.minecraft.net/";

    std::wstring utf8ToWide(std::string_view value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        if (size <= 0) return {};
        std::wstring converted(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            converted.data(), size);
        return converted;
    }

    const JsonValue* child(const JsonValue* parent, std::string_view key)
    {
        return parent == nullptr ? nullptr : parent->find(key);
    }

    std::string text(const JsonValue* value)
    {
        const auto* string = value == nullptr ? nullptr : value->stringValue();
        return string == nullptr ? std::string() : *string;
    }

    std::uint64_t unsignedNumber(const JsonValue* value)
    {
        return value == nullptr ? 0 : static_cast<std::uint64_t>(value->numberValue());
    }

    bool libraryAllowed(const JsonValue& library)
    {
        const auto* rules = child(&library, "rules");
        const auto* entries = rules == nullptr ? nullptr : rules->arrayValue();
        if (entries == nullptr) return true;

        bool allowed = false;
        for (const auto& rule : *entries)
        {
            const auto osName = text(child(child(&rule, "os"), "name"));
            const bool matches = osName.empty() || osName == "windows";
            if (matches)
            {
                allowed = text(child(&rule, "action")) == "allow";
            }
        }
        return allowed;
    }

    std::optional<VerifiedDownload> createDownload(
        const JsonValue* descriptor,
        const std::filesystem::path& destinationRoot)
    {
        const auto relativePath = text(child(descriptor, "path"));
        const auto url = text(child(descriptor, "url"));
        const auto sha1 = text(child(descriptor, "sha1"));
        if (relativePath.empty() || url.empty() || sha1.empty()) return std::nullopt;
        return VerifiedDownload{
            utf8ToWide(url),
            destinationRoot / std::filesystem::path(utf8ToWide(relativePath)),
            sha1,
            unsignedNumber(child(descriptor, "size")),
            FileHashAlgorithm::Sha1};
    }

    std::optional<JsonValue> parseDocument(
        const std::string& content, std::wstring& errorMessage)
    {
        std::string parseError;
        auto document = JsonValue::parse(content, &parseError);
        if (!document)
        {
            errorMessage = L"Manifesto JSON inválido: " + utf8ToWide(parseError);
        }
        return document;
    }
}

MinecraftInstallPlanResult MinecraftManifestService::createPlan(
    const std::filesystem::path& gameDirectory) const
{
    HttpDownloadClient http;
    const auto manifestResponse = http.getText(VersionManifestUrl);
    if (!manifestResponse.success)
    {
        return {false, L"Não foi possível obter o manifesto oficial do Minecraft: " +
                           manifestResponse.errorMessage, {}};
    }

    std::wstring parseError;
    const auto manifest = parseDocument(manifestResponse.content, parseError);
    if (!manifest) return {false, parseError, {}};

    std::string versionUrl;
    const auto* versions = child(&*manifest, "versions");
    if (const auto* entries = versions == nullptr ? nullptr : versions->arrayValue())
    {
        for (const auto& entry : *entries)
        {
            if (text(child(&entry, "id")) == "1.20.1")
            {
                versionUrl = text(child(&entry, "url"));
                break;
            }
        }
    }
    if (versionUrl.empty())
    {
        return {false, L"Minecraft 1.20.1 não foi encontrado no manifesto oficial.", {}};
    }

    const auto versionResponse = http.getText(utf8ToWide(versionUrl));
    if (!versionResponse.success)
    {
        return {false, L"Não foi possível obter os metadados do Minecraft 1.20.1.", {}};
    }
    const auto version = parseDocument(versionResponse.content, parseError);
    if (!version) return {false, parseError, {}};

    MinecraftInstallPlan plan;
    plan.versionMetadata = versionResponse.content;
    const auto librariesRoot = gameDirectory / L"libraries";

    const auto* client = child(child(&*version, "downloads"), "client");
    const auto clientUrl = text(child(client, "url"));
    const auto clientHash = text(child(client, "sha1"));
    if (clientUrl.empty() || clientHash.empty())
    {
        return {false, L"O manifesto não contém o cliente Minecraft 1.20.1.", {}};
    }
    const auto clientPath = gameDirectory / L"versions" / LauncherConfig::MinecraftVersion /
                            L"1.20.1.jar";
    plan.downloads.push_back({
        utf8ToWide(clientUrl), clientPath, clientHash,
        unsignedNumber(child(client, "size")), FileHashAlgorithm::Sha1});
    plan.classpathEntries.push_back(clientPath);

    const auto* libraries = child(&*version, "libraries");
    if (const auto* entries = libraries == nullptr ? nullptr : libraries->arrayValue())
    {
        for (const auto& library : *entries)
        {
            if (!libraryAllowed(library)) continue;
            const auto libraryName = text(child(&library, "name"));
            const bool alternateWindowsArchitecture =
                libraryName.ends_with(":natives-windows-x86") ||
                libraryName.ends_with(":natives-windows-arm64");
            if (alternateWindowsArchitecture) continue;

            const auto* downloads = child(&library, "downloads");
            if (auto artifact = createDownload(child(downloads, "artifact"), librariesRoot))
            {
                // Manifestos novos representam a nativa x64 como uma biblioteca
                // separada; manifestos antigos usam natives/classifiers.
                if (libraryName.ends_with(":natives-windows"))
                {
                    plan.nativeArchives.push_back(artifact->destination);
                }
                else
                {
                    plan.classpathEntries.push_back(artifact->destination);
                }
                plan.downloads.push_back(std::move(*artifact));
            }

            auto classifierName = text(child(child(&library, "natives"), "windows"));
            if (!classifierName.empty())
            {
                const auto marker = classifierName.find("${arch}");
                if (marker != std::string::npos) classifierName.replace(marker, 7, "64");
                if (auto native = createDownload(
                        child(child(downloads, "classifiers"), classifierName), librariesRoot))
                {
                    plan.nativeArchives.push_back(native->destination);
                    plan.downloads.push_back(std::move(*native));
                }
            }
        }
    }

    const auto* assetIndex = child(&*version, "assetIndex");
    const auto assetUrl = text(child(assetIndex, "url"));
    const auto assetHash = text(child(assetIndex, "sha1"));
    if (assetUrl.empty() || assetHash.empty())
    {
        return {false, L"O manifesto não contém o índice de assets do Minecraft.", {}};
    }
    const auto assetResponse = http.getText(utf8ToWide(assetUrl));
    if (!assetResponse.success)
    {
        return {false, L"Não foi possível obter o índice de assets do Minecraft.", {}};
    }
    plan.assetIndexMetadata = assetResponse.content;
    const auto assets = parseDocument(assetResponse.content, parseError);
    if (!assets) return {false, parseError, {}};

    const auto* objects = child(&*assets, "objects");
    if (const auto* entries = objects == nullptr ? nullptr : objects->objectValue())
    {
        for (const auto& [name, descriptor] : *entries)
        {
            (void)name;
            const auto hash = text(child(&descriptor, "hash"));
            if (hash.size() < 2) continue;
            const auto relative = hash.substr(0, 2) + "/" + hash;
            plan.downloads.push_back({
                std::wstring(AssetObjectBaseUrl) + utf8ToWide(relative),
                gameDirectory / L"assets" / L"objects" / utf8ToWide(relative),
                hash,
                unsignedNumber(child(&descriptor, "size")),
                FileHashAlgorithm::Sha1});
        }
    }

    const auto* loggingFile = child(child(child(&*version, "logging"), "client"), "file");
    if (auto logging = createDownload(loggingFile, gameDirectory / L"assets" / L"log_configs"))
    {
        plan.downloads.push_back(std::move(*logging));
    }

    return {true, L"Plano oficial do Minecraft preparado.", std::move(plan)};
}
