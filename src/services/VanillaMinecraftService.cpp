#include "services/VanillaMinecraftService.h"

#include "config/LauncherConfig.h"
#include "config/LauncherSettings.h"
#include "services/HttpDownloadClient.h"
#include "services/JavaRuntimePackage.h"
#include "services/MinecraftManifestService.h"
#include "services/VerifiedDownloader.h"
#include "utils/ArchiveExtractor.h"
#include "utils/JsonValue.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view VersionId = "1.8.9";
    constexpr const wchar_t* ReadyMarker = L"eternal-vanilla-ready";
    constexpr const wchar_t* Cancelled = L"Instalação cancelada. Os arquivos completos serão reaproveitados.";

    struct Failure
    {
        InstallationFailure category = InstallationFailure::Configuration;
        std::wstring message;
    };

    std::wstring wide(std::string_view value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring converted(static_cast<std::size_t>(std::max(size, 0)), L'\0');
        if (size > 0) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), converted.data(), size);
        return converted;
    }

    std::string utf8(std::wstring_view value)
    {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string converted(static_cast<std::size_t>(std::max(size, 0)), '\0');
        if (size > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), converted.data(), size, nullptr, nullptr);
        return converted;
    }

    bool writeText(const std::filesystem::path& file, const std::string& content)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output << content;
        return static_cast<bool>(output);
    }

    std::string text(const JsonValue* value)
    {
        const auto* string = value ? value->stringValue() : nullptr;
        return string ? *string : std::string{};
    }

    // Mojang's config is what disables the JNDI lookups of this old log4j (Log4Shell).
    // The game must never start without it, so a missing entry is a failure.
    std::optional<VerifiedDownload> loggingConfig(const std::string& versionMetadata, const std::filesystem::path& game)
    {
        const auto document = JsonValue::parse(versionMetadata);
        if (!document) return std::nullopt;
        const auto* logging = document->find("logging");
        const auto* client = logging ? logging->find("client") : nullptr;
        const auto* file = client ? client->find("file") : nullptr;
        if (!file) return std::nullopt;
        const auto url = text(file->find("url"));
        const auto sha1 = text(file->find("sha1"));
        const auto* size = file->find("size");
        if (text(file->find("id")) != "client-1.7.xml" || url.empty() || sha1.empty() || !size) return std::nullopt;
        return VerifiedDownload{wide(url), game / L"assets" / L"log_configs" / L"client-1.7.xml", sha1,
                                static_cast<std::uint64_t>(size->numberValue()), FileHashAlgorithm::Sha1};
    }

    std::optional<Failure> ensureJava8(const std::filesystem::path& data, std::stop_token stop,
                                       const InstallationProgressCallback& progress)
    {
        const auto runtime = data / LauncherConfig::Java8RuntimeDirectoryName;
        if (std::filesystem::is_regular_file(runtime / L"bin" / L"java.exe")) {
            progress({InstallationStage::Java, InstallationActivity::Checking, InstallationStepState::Available, L"Java 8 já disponível."});
            return std::nullopt;
        }
        progress({InstallationStage::Java, InstallationActivity::Checking, InstallationStepState::Active, L"Consultando a versão portátil do Java 8..."});
        HttpDownloadClient http;
        const auto metadata = http.getText(LauncherConfig::Java8RuntimeMetadataUrl, stop);
        if (!metadata.success) return Failure{metadata.failure, L"Não foi possível consultar o Java 8. Confira sua internet e tente novamente."};
        const auto package = JavaRuntimePackage::parse(metadata.content);
        if (!package) return Failure{InstallationFailure::Configuration, L"A resposta do provedor do Java é inválida."};

        const auto archive = data / L"downloads" / L"temurin-8.zip.part";
        const auto download = http.downloadFile(package->url, archive, stop, [&](std::uint64_t received, std::uint64_t total) {
            const auto expected = total > 0 ? total : package->size;
            InstallationProgress value;
            value.activity = InstallationActivity::Downloading;
            value.message = L"Baixando Java 8 portátil...";
            value.receivedBytes = received;
            value.totalFiles = 1;
            if (expected) {
                value.totalBytes = expected;
                value.percent = std::min(99, static_cast<int>(static_cast<double>(received) * 100 / static_cast<double>(expected)));
            }
            progress(value);
        });
        if (!download.success) return Failure{download.failure, L"Não foi possível baixar o Java 8. Confira sua internet e tente novamente."};

        std::error_code ignored;
        progress({InstallationStage::Java, InstallationActivity::Verifying, InstallationStepState::Active, L"Verificando a integridade do Java..."});
        if (!package->matchesFile(archive, stop)) {
            std::filesystem::remove(archive, ignored);
            return Failure{InstallationFailure::Integrity, L"O arquivo do Java 8 não passou na verificação. Tente novamente."};
        }

        progress({InstallationStage::Java, InstallationActivity::Extracting, InstallationStepState::Active, L"Extraindo o Java 8..."});
        const auto staging = data / L"runtime-8-staging";
        std::filesystem::remove_all(staging, ignored);
        std::filesystem::create_directories(staging);
        const bool extracted = ArchiveExtractor::extract(archive, staging, stop);
        std::filesystem::remove(archive, ignored);
        std::optional<std::filesystem::path> root;
        if (extracted) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(staging)) {
                if (entry.is_regular_file() && entry.path().filename() == L"java.exe" && entry.path().parent_path().filename() == L"bin") {
                    root = entry.path().parent_path().parent_path();
                    break;
                }
            }
        }
        if (!root) {
            std::filesystem::remove_all(staging, ignored);
            return Failure{InstallationFailure::Process, L"Não foi possível extrair o Java 8."};
        }
        std::filesystem::remove_all(runtime, ignored);
        std::filesystem::rename(*root, runtime);
        std::filesystem::remove_all(staging, ignored);
        progress({InstallationStage::Java, InstallationActivity::Publishing, InstallationStepState::Complete, L"Java 8 portátil instalado."});
        return std::nullopt;
    }
}

std::filesystem::path VanillaMinecraftService::gameDirectory()
{
    return LauncherSettingsStore::dataDirectory() / LauncherConfig::VanillaGameDirectoryName;
}

std::filesystem::path VanillaMinecraftService::javaExecutable()
{
    return LauncherSettingsStore::dataDirectory() / LauncherConfig::Java8RuntimeDirectoryName / L"bin" / L"java.exe";
}

bool VanillaMinecraftService::isInstalled()
{
    std::error_code ignored;
    return std::filesystem::is_regular_file(gameDirectory() / ReadyMarker, ignored);
}

InstallationResult VanillaMinecraftService::prepare(std::stop_token stop, const InstallationProgressCallback& callback) const
{
    const InstallationProgressCallback progress = [&](const InstallationProgress& value) { if (callback) callback(value); };
    const auto cancelled = [] { return InstallationResult{InstallationOutcome::Cancelled, InstallationFailure::None, Cancelled}; };
    const auto failed = [&](const Failure& failure) {
        if (stop.stop_requested()) return cancelled();
        return InstallationResult{InstallationOutcome::Failed,
            failure.category == InstallationFailure::None ? InstallationFailure::Configuration : failure.category, failure.message, failure.message};
    };
    const auto skipUnusedStages = [&] {
        progress({InstallationStage::Forge, InstallationActivity::Checking, InstallationStepState::Available, L"Não usado na versão 1.8.9."});
        progress({InstallationStage::Mods, InstallationActivity::Checking, InstallationStepState::Available, L"A versão 1.8.9 não usa os mods do grupo."});
    };

    try {
        const auto data = LauncherSettingsStore::dataDirectory();
        const auto game = gameDirectory();
        std::filesystem::create_directories(game);
        if (const auto failure = ensureJava8(data, stop, progress)) return failed(*failure);
        if (stop.stop_requested()) return cancelled();

        progress({InstallationStage::Minecraft, InstallationActivity::Checking, InstallationStepState::Active, L"Consultando os manifestos oficiais do Minecraft..."});
        auto planResult = MinecraftManifestService{}.createPlan(game, stop, VersionId);
        if (stop.stop_requested()) return cancelled();
        const auto marker = game / ReadyMarker;
        if (!planResult.success && std::filesystem::is_regular_file(marker)) {
            // Already installed and verified once: playing does not depend on reaching Mojang.
            progress({InstallationStage::Minecraft, InstallationActivity::Checking, InstallationStepState::Available, L"Usando os arquivos já instalados."});
            progress({InstallationStage::Natives, InstallationActivity::Checking, InstallationStepState::Available, L"Bibliotecas nativas já preparadas."});
            skipUnusedStages();
            progress({InstallationStage::Validation, InstallationActivity::Verifying, InstallationStepState::Complete, L"Iniciando Minecraft 1.8.9..."});
            return {InstallationOutcome::Success, InstallationFailure::None, L"Instalação local reaproveitada."};
        }
        if (!planResult.success)
            return failed({planResult.failure, L"Não foi possível consultar os arquivos oficiais do Minecraft. Confira sua internet e tente novamente."});

        std::error_code ignored;
        std::filesystem::remove(marker, ignored);
        auto& plan = planResult.plan;
        const auto logging = loggingConfig(plan.versionMetadata, game);
        if (!logging) return failed({InstallationFailure::Configuration, L"O manifesto oficial não trouxe a configuração segura de logs da 1.8.9."});
        plan.downloads.push_back(*logging);
        // Several asset names can point at the same object; each file is fetched once.
        std::set<std::filesystem::path> seen;
        std::vector<VerifiedDownload> unique;
        unique.reserve(plan.downloads.size());
        for (auto& file : plan.downloads)
            if (seen.insert(file.destination).second) unique.push_back(std::move(file));

        const std::wstring version(LauncherConfig::VanillaVersion);
        if (!writeText(game / L"versions" / version / (version + L".json"), plan.versionMetadata) ||
            !writeText(game / L"assets" / L"indexes" / (std::wstring(LauncherConfig::VanillaAssetIndex) + L".json"), plan.assetIndexMetadata))
            return failed({InstallationFailure::Disk, L"Não foi possível gravar os arquivos. Confira espaço livre e permissões da pasta."});

        const auto downloaded = VerifiedDownloader{}.downloadAll(unique, stop, [&](const InstallationProgress& value) {
            auto copy = value;
            copy.stage = InstallationStage::Minecraft;
            if (value.activity == InstallationActivity::Downloading) copy.message = L"Baixando Minecraft 1.8.9, bibliotecas e assets...";
            progress(copy);
        });
        if (!downloaded.success) return failed({downloaded.failure, downloaded.message});
        progress({InstallationStage::Minecraft, InstallationActivity::Verifying,
            downloaded.allAvailable ? InstallationStepState::Available : InstallationStepState::Complete, L"Minecraft e assets verificados."});

        progress({InstallationStage::Natives, InstallationActivity::Extracting, InstallationStepState::Active, L"Preparando bibliotecas nativas..."});
        const auto natives = game / L"natives";
        bool nativesReused = false;
        if (!ArchiveExtractor::prepareNatives(plan.nativeArchives, natives, L"lwjgl64.dll", stop, nativesReused))
            return failed({InstallationFailure::Process, L"Não foi possível extrair uma biblioteca nativa."});
        if (!std::filesystem::is_regular_file(natives / L"lwjgl64.dll"))
            return failed({InstallationFailure::Integrity, L"As bibliotecas nativas da 1.8.9 não foram preparadas."});
        if (!nativesReused) ArchiveExtractor::rememberNatives(plan.nativeArchives, natives);
        progress({InstallationStage::Natives, InstallationActivity::Extracting,
            nativesReused ? InstallationStepState::Available : InstallationStepState::Complete,
            nativesReused ? L"Bibliotecas nativas já preparadas." : L"Bibliotecas nativas preparadas."});
        skipUnusedStages();

        std::string classpath;
        for (const auto& entry : plan.classpathEntries) classpath += utf8(entry.wstring()) + "\n";
        if (!writeText(game / L"eternal-classpath.txt", classpath) || !writeText(marker, "ok\n"))
            return failed({InstallationFailure::Disk, L"Não foi possível salvar a instalação do jogo."});
        progress({InstallationStage::Validation, InstallationActivity::Verifying, InstallationStepState::Complete, L"Iniciando Minecraft 1.8.9..."});
        return {InstallationOutcome::Success, InstallationFailure::None, L"Instalação concluída e verificada.", {}, unique.size()};
    } catch (const std::exception&) {
        return failed({InstallationFailure::Disk, L"Não foi possível gravar os arquivos. Confira espaço livre e permissões da pasta."});
    }
}
