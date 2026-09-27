#include "services/GameInstallationService.h"

#include "config/LauncherConfig.h"
#include "services/MinecraftManifestService.h"
#include "services/MinecraftServerListService.h"
#include "services/VerifiedDownloader.h"
#include "utils/ArchiveExtractor.h"
#include "utils/JsonValue.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace
{
    std::wstring environmentVariable(const wchar_t* name)
    {
        wchar_t* value = nullptr;
        std::size_t length = 0;
        if (_wdupenv_s(&value, &length, name) != 0 || value == nullptr) return {};
        std::wstring result(value);
        std::free(value);
        return result;
    }

    std::string wideToUtf8(std::wstring_view value)
    {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);
        std::string converted(static_cast<std::size_t>(std::max(size, 0)), '\0');
        if (size > 0)
        {
            WideCharToMultiByte(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                converted.data(), size, nullptr, nullptr);
        }
        return converted;
    }

    std::wstring utf8ToWide(std::string_view value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        std::wstring converted(static_cast<std::size_t>(std::max(size, 0)), L'\0');
        if (size > 0)
        {
            MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                converted.data(), size);
        }
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

    void writeTextFile(const std::filesystem::path& path, std::string_view content)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!output) throw std::runtime_error("Falha ao gravar metadados da instalação.");
    }

    std::wstring quote(std::wstring_view value)
    {
        return L"\"" + std::wstring(value) + L"\"";
    }

    bool runForgeInstaller(const std::filesystem::path& java,
                           const std::filesystem::path& installer,
                           const std::filesystem::path& gameDirectory,
                           std::stop_token stopToken)
    {
        std::wstring command = quote(java.wstring()) + L" -jar " + quote(installer.wstring()) +
                               L" --installClient " + quote(gameDirectory.wstring());
        STARTUPINFO startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcess(java.c_str(), command.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, gameDirectory.c_str(),
                           &startup, &process)) return false;

        CloseHandle(process.hThread);
        while (WaitForSingleObject(process.hProcess, 200) == WAIT_TIMEOUT)
        {
            if (stopToken.stop_requested())
            {
                TerminateProcess(process.hProcess, ERROR_CANCELLED);
                WaitForSingleObject(process.hProcess, INFINITE);
                break;
            }
        }
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hProcess);
        return !stopToken.stop_requested() && exitCode == 0;
    }

    bool isSafeRelativePath(const std::filesystem::path& path)
    {
        if (path.empty() || path.is_absolute() || path.has_root_name()) return false;
        return std::none_of(path.begin(), path.end(), [](const auto& part)
        {
            return part == L"..";
        });
    }

    std::vector<VerifiedDownload> privateFiles(
        std::wstring_view manifestText,
        const std::filesystem::path& gameDirectory,
        std::wstring& error)
    {
        std::vector<VerifiedDownload> files;
        std::string parseError;
        const auto manifest = JsonValue::parse(wideToUtf8(manifestText), &parseError);
        if (!manifest)
        {
            error = L"Manifesto privado inválido: " + utf8ToWide(parseError);
            return files;
        }

        const auto* entriesValue = child(&*manifest, "files");
        const auto* entries = entriesValue == nullptr ? nullptr : entriesValue->arrayValue();
        if (entries == nullptr) return files;
        for (const auto& entry : *entries)
        {
            const auto relativeText = text(child(&entry, "path"));
            auto hash = text(child(&entry, "sha256"));
            const bool validHash = hash.size() == 64 &&
                std::all_of(hash.begin(), hash.end(), [](unsigned char character)
                {
                    return std::isxdigit(character) != 0;
                });
            if (relativeText.empty() || !validHash)
            {
                error = L"Um arquivo do manifesto privado não possui path/sha256 válido.";
                return {};
            }
            std::transform(hash.begin(), hash.end(), hash.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });

            const std::filesystem::path relative(utf8ToWide(relativeText));
            if (!isSafeRelativePath(relative))
            {
                error = L"O manifesto tentou usar um caminho inseguro: " + relative.wstring();
                return {};
            }

            auto url = text(child(&entry, "url"));
            if (url.empty())
            {
                std::wstring base(LauncherConfig::FileServerBaseUrl);
                if (!base.ends_with(L'/')) base += L'/';
                base += utf8ToWide(relativeText);
                files.push_back({base, gameDirectory / relative, hash,
                                 static_cast<std::uint64_t>(child(&entry, "size")
                                     ? child(&entry, "size")->numberValue() : 0),
                                 FileHashAlgorithm::Sha256});
            }
            else
            {
                files.push_back({utf8ToWide(url), gameDirectory / relative, hash,
                                 static_cast<std::uint64_t>(child(&entry, "size")
                                     ? child(&entry, "size")->numberValue() : 0),
                                 FileHashAlgorithm::Sha256});
            }
        }
        return files;
    }

    void appendForgeClasspath(const std::filesystem::path& profile,
                              const std::filesystem::path& libraries,
                              std::vector<std::filesystem::path>& classpath)
    {
        std::ifstream input(profile, std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(input)), {});
        const auto document = JsonValue::parse(content);
        if (!document) throw std::runtime_error("Perfil Forge inválido após instalação.");
        const auto* librariesValue = child(&*document, "libraries");
        const auto* entries = librariesValue == nullptr ? nullptr : librariesValue->arrayValue();
        if (entries == nullptr) return;
        for (const auto& entry : *entries)
        {
            const auto path = text(child(child(child(&entry, "downloads"), "artifact"), "path"));
            if (!path.empty()) classpath.push_back(libraries / utf8ToWide(path));
        }
    }

    bool copyNativeLibrariesToRoot(const std::filesystem::path& natives)
    {
        std::vector<std::filesystem::path> nestedLibraries;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(natives))
        {
            if (entry.is_regular_file() && entry.path().extension() == L".dll" &&
                entry.path().parent_path() != natives)
            {
                nestedLibraries.push_back(entry.path());
            }
        }

        // Os manifestos atuais da Mojang armazenam os DLLs em subdiretórios
        // dentro dos JARs nativos. A JVM, porém, recebe a raiz em java.library.path.
        for (const auto& library : nestedLibraries)
        {
            std::error_code error;
            std::filesystem::copy_file(
                library, natives / library.filename(),
                std::filesystem::copy_options::overwrite_existing, error);
            if (error) return false;
        }
        return std::any_of(
            std::filesystem::directory_iterator(natives),
            std::filesystem::directory_iterator(),
            [](const auto& entry)
            {
                return entry.is_regular_file() && entry.path().extension() == L".dll";
            });
    }

    bool forgeInstallationReady(const std::filesystem::path& gameDirectory)
    {
        const auto libraries = gameDirectory / L"libraries";
        const std::array required = {
            gameDirectory / L"versions/1.20.1-forge-47.4.10/1.20.1-forge-47.4.10.json",
            libraries / L"net/minecraftforge/forge/1.20.1-47.4.10/forge-1.20.1-47.4.10-universal.jar",
            libraries / L"net/minecraftforge/fmlcore/1.20.1-47.4.10/fmlcore-1.20.1-47.4.10.jar",
            libraries / L"net/minecraftforge/javafmllanguage/1.20.1-47.4.10/javafmllanguage-1.20.1-47.4.10.jar",
            libraries / L"net/minecraftforge/lowcodelanguage/1.20.1-47.4.10/lowcodelanguage-1.20.1-47.4.10.jar",
            libraries / L"net/minecraftforge/mclanguage/1.20.1-47.4.10/mclanguage-1.20.1-47.4.10.jar"
        };
        return std::all_of(required.begin(), required.end(), [](const auto& path)
        {
            return std::filesystem::is_regular_file(path);
        });
    }
}

GameInstallationResult GameInstallationService::ensureInstalled(
    std::wstring_view privateManifest,
    std::stop_token stopToken,
    const GameInstallationProgressCallback& progress)
{
    const auto report = [&progress](int percent, std::wstring_view message)
    {
        if (progress) progress(percent, message);
    };

    try
    {
        const auto gamePath = gameDirectory();
        const auto dataPath = dataDirectory();
        std::filesystem::create_directories(gamePath);

        report(2, L"Consultando os manifestos oficiais do Minecraft...");
        MinecraftManifestService manifests;
        auto planResult = manifests.createPlan(gamePath);
        if (!planResult.success) return {false, planResult.message, 0};
        auto& plan = planResult.plan;

        writeTextFile(gamePath / L"versions" / L"1.20.1" / L"1.20.1.json",
                      plan.versionMetadata);
        writeTextFile(gamePath / L"assets" / L"indexes" / L"5.json",
                      plan.assetIndexMetadata);

        report(5, L"Baixando Minecraft, bibliotecas e assets...");
        VerifiedDownloader downloader;
        const auto baseDownload = downloader.downloadAll(
            plan.downloads, stopToken,
            [&](std::uint64_t completed, std::uint64_t total)
            {
                const int value = total > 0
                    ? 5 + static_cast<int>(completed * 65 / total)
                    : 5;
                report(std::min(value, 70), L"Baixando Minecraft, bibliotecas e assets...");
            });
        if (!baseDownload.success) return {false, baseDownload.message, 0};

        report(72, L"Preparando bibliotecas nativas...");
        const auto natives = gamePath / L"natives";
        std::error_code ignored;
        std::filesystem::remove_all(natives, ignored);
        std::filesystem::create_directories(natives);
        for (const auto& archive : plan.nativeArchives)
        {
            if (!ArchiveExtractor::extract(archive, natives, stopToken))
            {
                return {false, L"Não foi possível extrair uma biblioteca nativa.", 0};
            }
        }
        if (!copyNativeLibrariesToRoot(natives))
        {
            return {false, L"Nenhuma biblioteca nativa compatível foi preparada.", 0};
        }

        const auto forgeInstaller = dataPath / L"downloads" /
                                    L"forge-1.20.1-47.4.10-installer.jar";
        report(75, L"Baixando o instalador oficial do Forge...");
        const std::vector<VerifiedDownload> forgeDownload = {{
            std::wstring(LauncherConfig::ForgeInstallerUrl), forgeInstaller,
            std::string(LauncherConfig::ForgeInstallerSha1), 6078070,
            FileHashAlgorithm::Sha1}};
        const auto forgeFileResult = downloader.downloadAll(
            forgeDownload, stopToken,
            [&](std::uint64_t completed, std::uint64_t total)
            {
                report(total > 0 ? 75 + static_cast<int>(completed * 5 / total) : 75,
                       L"Baixando o instalador oficial do Forge...");
            }, 1);
        if (!forgeFileResult.success) return {false, forgeFileResult.message, 0};

        const auto forgeProfile = gamePath / L"versions" / LauncherConfig::VersionProfile /
                                  L"1.20.1-forge-47.4.10.json";
        if (!forgeInstallationReady(gamePath))
        {
            writeTextFile(gamePath / L"launcher_profiles.json", "{\"profiles\":{}}");
            report(81, L"Instalando Forge 47.4.10...");
            const auto java = dataPath / L"runtime" / L"bin" / L"java.exe";
            if (!runForgeInstaller(java, forgeInstaller, gamePath, stopToken))
            {
                return {false, L"O instalador do Forge não concluiu a preparação.", 0};
            }
        }

        report(94, L"Sincronizando mods e configurações...");
        std::wstring manifestError;
        const auto files = privateFiles(privateManifest, gamePath, manifestError);
        if (!manifestError.empty()) return {false, manifestError, 0};
        const auto privateResult = downloader.downloadAll(
            files, stopToken,
            [&](std::uint64_t completed, std::uint64_t total)
            {
                report(total > 0 ? 94 + static_cast<int>(completed * 5 / total) : 99,
                       L"Sincronizando mods e configurações...");
            });
        if (!privateResult.success) return {false, privateResult.message, 0};

        appendForgeClasspath(forgeProfile, gamePath / L"libraries", plan.classpathEntries);
        std::sort(plan.classpathEntries.begin(), plan.classpathEntries.end());
        plan.classpathEntries.erase(
            std::unique(plan.classpathEntries.begin(), plan.classpathEntries.end()),
            plan.classpathEntries.end());
        std::ofstream classpath(gamePath / L"eternal-classpath.txt", std::ios::trunc);
        for (const auto& entry : plan.classpathEntries)
        {
            classpath << wideToUtf8(entry.wstring()) << '\n';
        }
        if (!classpath) return {false, L"Não foi possível salvar o classpath do jogo.", 0};

        report(99, L"Adicionando o Eternal SMP à lista Multiplayer...");
        MinecraftServerListService serverList;
        std::wstring serverError;
        if (!serverList.ensureServerPresent(
                gamePath, LauncherConfig::GameServerName,
                LauncherConfig::GameServerAddress, stopToken, serverError))
        {
            return {false, serverError, 0};
        }

        report(100, L"Minecraft 1.20.1 com Forge 47.4.10 está pronto.");
        return {true, L"Instalação concluída e verificada.", files.size()};
    }
    catch (const std::exception& error)
    {
        return {false, L"Falha durante a instalação: " + utf8ToWide(error.what()), 0};
    }
}

std::filesystem::path GameInstallationService::dataDirectory() const
{
    const auto appData = environmentVariable(L"APPDATA");
    return appData.empty()
        ? std::filesystem::current_path() / LauncherConfig::DataDirectoryName
        : std::filesystem::path(appData) / LauncherConfig::DataDirectoryName;
}

std::filesystem::path GameInstallationService::gameDirectory() const
{
    return dataDirectory() / LauncherConfig::GameDirectoryName;
}
