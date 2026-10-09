#include "services/GameInstallationService.h"
#include "utils/InstallationProcess.h"

#include "config/LauncherConfig.h"
#include "services/MinecraftManifestService.h"
#include "services/ModCleanup.h"
#include "services/JavaRuntimeService.h"
#include "services/VerifiedDownloader.h"
#include "utils/ArchiveExtractor.h"
#include "utils/JsonValue.h"
#include "utils/UrlEncoding.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

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

    struct ForgeInstallResult
    {
        bool success;
        bool retryable;
        std::wstring message;
        bool javaStartFailure = false;
    };

    ForgeInstallResult runForgeInstaller(const std::filesystem::path& java,
                                         const std::filesystem::path& installer,
                                         const std::filesystem::path& gameDirectory,
                                         std::stop_token stopToken,
                                         int attempt,
                                         bool alternateNetwork)
    {
        const auto logPath = gameDirectory / L"logs" / L"forge-installer.log";
        std::filesystem::create_directories(logPath.parent_path());
        {
            std::ofstream log(logPath, std::ios::binary |
                (attempt > 1 ? std::ios::app : std::ios::trunc));
            log << "\n[Eternal] Tentativa " << attempt
                << (alternateNetwork ? " (rede alternativa IPv4/IPv6)" : " (rede padrão)")
                << "\nJava: "
                << wideToUtf8(java.wstring()) << "\nInstalador: "
                << wideToUtf8(installer.wstring()) << "\nDestino: "
                << wideToUtf8(gameDirectory.wstring()) << "\n\n";
            if (!log)
            {
                return {false, false,
                        L"Não foi possível criar o log do Forge: " + logPath.wstring()};
            }
        }
        const auto failure = [&logPath](std::wstring reason, bool retryable = false,
                                        bool javaStartFailure = false) -> ForgeInstallResult
        {
            std::ofstream log(logPath, std::ios::binary | std::ios::app);
            log << "\n[Eternal] " << wideToUtf8(reason) << '\n';
            return {false, retryable,
                    std::move(reason) + L" Log: " + logPath.wstring(), javaStartFailure};
        };

        if(stopToken.stop_requested()) return failure(L"Instalação do Forge cancelada.");

        if (!std::filesystem::is_regular_file(java))
        {
            return failure(L"O java.exe portátil não existe: " + java.wstring(), false, true);
        }
        if (!std::filesystem::is_regular_file(installer))
        {
            return failure(L"O instalador Forge não existe: " + installer.wstring());
        }
        if (!std::filesystem::is_directory(gameDirectory))
        {
            return failure(L"A pasta do Minecraft não existe: " + gameDirectory.wstring());
        }

        SECURITY_ATTRIBUTES inherit{};
        inherit.nLength = sizeof(inherit);
        inherit.bInheritHandle = TRUE;
        const HANDLE output = CreateFileW(logPath.c_str(), FILE_APPEND_DATA,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output == INVALID_HANDLE_VALUE)
        {
            const DWORD error = GetLastError();
            return failure(L"Não foi possível abrir o log do Forge (erro Windows " +
                           std::to_wstring(error) + L").");
        }
        const HANDLE input = CreateFileW(L"NUL", GENERIC_READ,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (input == INVALID_HANDLE_VALUE)
        {
            const DWORD error = GetLastError();
            CloseHandle(output);
            return failure(L"Não foi possível preparar a entrada do Forge (erro Windows " +
                           std::to_wstring(error) + L").");
        }

        std::wstring command = quote(java.wstring());
        if (alternateNetwork) command += L" -Djava.net.preferIPv4Stack=false";
        command += L" -jar " + quote(installer.wstring()) +
                   L" --installClient " + quote(gameDirectory.wstring());
        STARTUPINFO startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input;
        startup.hStdOutput = output;
        startup.hStdError = output;
        PROCESS_INFORMATION process{};
        const BOOL started = CreateProcessW(java.c_str(), command.data(), nullptr, nullptr, TRUE,
                                            CREATE_NO_WINDOW, nullptr, gameDirectory.c_str(),
                                            &startup, &process);
        const DWORD startError = started ? ERROR_SUCCESS : GetLastError();
        CloseHandle(input);
        CloseHandle(output);
        if (!started)
        {
            std::ofstream log(logPath, std::ios::binary | std::ios::app);
            log << "[Eternal] Diagnóstico de caminhos: java.exe="
                << std::filesystem::is_regular_file(java)
                << ", instalador=" << std::filesystem::is_regular_file(installer)
                << ", destino=" << std::filesystem::is_directory(gameDirectory) << '\n';
            return failure(L"Não foi possível iniciar o instalador Forge (erro Windows " +
                           std::to_wstring(startError) + L").", false,
                           startError == ERROR_FILE_NOT_FOUND ||
                           startError == ERROR_PATH_NOT_FOUND ||
                           startError == ERROR_MOD_NOT_FOUND ||
                           startError == ERROR_BAD_EXE_FORMAT);
        }

        CloseHandle(process.hThread);
        const auto outcome=waitForInstallationProcess(process.hProcess,stopToken);
        CloseHandle(process.hProcess);
        if (outcome.cancelled) return failure(L"Instalação do Forge cancelada.");
        if (outcome.error)
        {
            return failure(L"Não foi possível consultar o resultado do Forge (erro Windows " +
                           std::to_wstring(outcome.error) +
                           L").");
        }
        if (outcome.exitCode != 0)
        {
            return failure(L"O instalador Forge terminou com código " +
                           std::to_wstring(outcome.exitCode) + L".", true);
        }
        std::ofstream log(logPath, std::ios::binary | std::ios::app);
        log << "\n[Eternal] Instalador Forge concluído com código 0.\n";
        return {true, false, L"Instalador Forge concluído."};
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
        if (entries == nullptr)
        {
            error = L"O manifesto privado não contém uma lista válida de arquivos.";
            return files;
        }
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
                base += utf8ToWide(encodeRelativeUrlPath(relativeText));
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
    std::wstring_view privateManifest,std::stop_token stopToken,const GameInstallationProgressCallback& progress)
{
    auto result=install(privateManifest,stopToken,progress);
    result.outcome=stopToken.stop_requested() ? InstallationOutcome::Cancelled : result.success ? InstallationOutcome::Success : InstallationOutcome::Failed;
    if(result.outcome==InstallationOutcome::Cancelled) {result.success=false;result.message=L"Instalação cancelada.";result.failure=InstallationFailure::None;}
    return result;
}

GameInstallationResult GameInstallationService::install(
    std::wstring_view privateManifest,
    std::stop_token stopToken,
    const GameInstallationProgressCallback& progress)
{
    const auto report = [&progress](InstallationStage stage,InstallationActivity activity,std::wstring_view message,
        InstallationStepState state=InstallationStepState::Active)
    {
        if (progress) progress({stage,activity,state,std::wstring(message)});
    };
    const auto transfer=[&](InstallationStage stage,std::wstring_view label) {
        return [&,stage,label](const InstallationProgress& value) {
            auto current=value;current.stage=stage;
            if(value.activity==InstallationActivity::Downloading) current.message=label;
            if(progress) progress(current);
        };
    };

    try
    {
        const auto gamePath = gameDirectory();
        const auto dataPath = dataDirectory();
        std::wstring settingsError;
        if (!LauncherSettingsStore::validate(settings_, settingsError)) return {false, settingsError, 0,InstallationFailure::Configuration};
        if(stopToken.stop_requested()) return {false,L"Instalação cancelada.",0};
        std::filesystem::create_directories(gamePath);
        writeTextFile(gamePath / L"eternal-instance.json", "{\"launcher\":\"Eternal\",\"schemaVersion\":1}\n");

        report(InstallationStage::Minecraft,InstallationActivity::Checking, L"Consultando os manifestos oficiais do Minecraft...");
        MinecraftManifestService manifests;
        auto planResult = manifests.createPlan(gamePath,stopToken);
        if (!planResult.success) return {false, planResult.message, 0,planResult.failure};
        if(stopToken.stop_requested()) return {false,L"Instalação cancelada.",0};
        auto& plan = planResult.plan;

        writeTextFile(gamePath / L"versions" / L"1.20.1" / L"1.20.1.json",
                      plan.versionMetadata);
        writeTextFile(gamePath / L"assets" / L"indexes" / L"5.json",
                      plan.assetIndexMetadata);

        VerifiedDownloader downloader;
        const auto baseDownload = downloader.downloadAll(
            plan.downloads, stopToken,transfer(InstallationStage::Minecraft,L"Baixando Minecraft, bibliotecas e assets..."));
        if (!baseDownload.success) return {false, baseDownload.message, 0,baseDownload.failure};
        report(InstallationStage::Minecraft,InstallationActivity::Verifying,L"Minecraft e assets verificados.",
            baseDownload.allAvailable ? InstallationStepState::Available : InstallationStepState::Complete);

        report(InstallationStage::Natives,InstallationActivity::Extracting, L"Preparando bibliotecas nativas...");
        if(stopToken.stop_requested()) return {false,L"Instalação cancelada.",0};
        const auto natives = gamePath / L"natives";
        std::error_code ignored;
        std::filesystem::remove_all(natives, ignored);
        std::filesystem::create_directories(natives);
        for (const auto& archive : plan.nativeArchives)
        {
            if (!ArchiveExtractor::extract(archive, natives, stopToken))
            {
                return {false, L"Não foi possível extrair uma biblioteca nativa.", 0,InstallationFailure::Process};
            }
        }
        if (!copyNativeLibrariesToRoot(natives))
        {
            return {false, L"Nenhuma biblioteca nativa compatível foi preparada.", 0,InstallationFailure::Integrity};
        }
        report(InstallationStage::Natives,InstallationActivity::Extracting,L"Bibliotecas nativas preparadas.",InstallationStepState::Complete);

        const auto forgeInstaller = dataPath / L"downloads" /
                                    L"forge-1.20.1-47.4.10-installer.jar";
        report(InstallationStage::Forge,InstallationActivity::Checking, L"Conferindo o Forge...");
        const std::vector<VerifiedDownload> forgeDownload = {{
            std::wstring(LauncherConfig::ForgeInstallerUrl), forgeInstaller,
            std::string(LauncherConfig::ForgeInstallerSha1), 6078070,
            FileHashAlgorithm::Sha1}};
        const auto forgeFileResult = downloader.downloadAll(
            forgeDownload, stopToken,transfer(InstallationStage::Forge,L"Baixando o instalador oficial do Forge..."), 1);
        if (!forgeFileResult.success) return {false, forgeFileResult.message, 0,forgeFileResult.failure};

        const auto forgeProfile = gamePath / L"versions" / LauncherConfig::VersionProfile /
                                  L"1.20.1-forge-47.4.10.json";
        const bool forgeAvailable=forgeInstallationReady(gamePath);
        if (!forgeAvailable)
        {
            writeTextFile(gamePath / L"launcher_profiles.json", "{\"profiles\":{}}");
            report(InstallationStage::Forge,InstallationActivity::Installing, L"Executando instalador Forge 47.4.10...");
            const auto java = settings_.javaExecutable.empty() ? dataPath / L"runtime" / L"bin" / L"java.exe"
                : settings_.javaExecutable.parent_path() / L"java.exe";
            int attempt = 1;
            auto forgeResult = runForgeInstaller(
                java, forgeInstaller, gamePath, stopToken, attempt, false);
            if (forgeResult.javaStartFailure && settings_.javaExecutable.empty() && !stopToken.stop_requested())
            {
                report(InstallationStage::Forge,InstallationActivity::Installing, L"Reparando o Java portátil para instalar o Forge...");
                JavaRuntimeService runtime;
                const auto repair = runtime.reinstall(stopToken,
                    [&](const InstallationProgress& value)
                    {
                        auto current=value;current.stage=InstallationStage::Forge;current.state=InstallationStepState::Active;
                        current.transferId=1;current.message=L"Reparando Java: "+value.message;
                        if(progress) progress(current);
                    });
                if (!repair.success)
                {
                    std::ofstream log(gamePath / L"logs" / L"forge-installer.log",
                                      std::ios::binary | std::ios::app);
                    log << "[Eternal] Reparo do Java falhou: "
                        << wideToUtf8(repair.message) << '\n';
                    return {false, L"Não foi possível reparar o Java: " + repair.message +
                                   L" Log: " +
                                   (gamePath / L"logs" / L"forge-installer.log").wstring(), 0,repair.failure};
                }
                report(InstallationStage::Forge,InstallationActivity::Installing, L"Tentando instalar o Forge com o Java reparado...");
                forgeResult = runForgeInstaller(
                    java, forgeInstaller, gamePath, stopToken, ++attempt, false);
            }
            const bool incompleteForge = forgeResult.success && !forgeInstallationReady(gamePath);
            if (((!forgeResult.success && forgeResult.retryable) || incompleteForge) &&
                !stopToken.stop_requested())
            {
                if (incompleteForge)
                {
                    std::ofstream log(gamePath / L"logs" / L"forge-installer.log",
                                      std::ios::binary | std::ios::app);
                    log << "[Eternal] Arquivos essenciais ausentes após a primeira tentativa.\n";
                }
                report(InstallationStage::Forge,InstallationActivity::Installing, L"Repetindo a instalação Forge com rede alternativa...");
                forgeResult = runForgeInstaller(
                    java, forgeInstaller, gamePath, stopToken, ++attempt, true);
            }
            if (!forgeResult.success)
            {
                return {false, forgeResult.message, 0,InstallationFailure::Process};
            }
            if (!forgeInstallationReady(gamePath))
            {
                return {false, L"O Forge terminou, mas faltam arquivos essenciais. Log: " +
                               (gamePath / L"logs" / L"forge-installer.log").wstring(), 0,InstallationFailure::Process};
            }
        }
        report(InstallationStage::Forge,InstallationActivity::Verifying,L"Forge verificado.",
            forgeAvailable ? InstallationStepState::Available : InstallationStepState::Complete);

        report(InstallationStage::Mods,InstallationActivity::Checking, L"Sincronizando mods e configurações...");
        std::wstring manifestError;
        const auto files = privateFiles(privateManifest, gamePath, manifestError);
        if (!manifestError.empty()) return {false, manifestError, 0,InstallationFailure::Configuration};
        const auto privateResult = downloader.downloadAll(
            files, stopToken,transfer(InstallationStage::Mods,L"Baixando mods e configurações..."));
        if (!privateResult.success) return {false, privateResult.message, 0,privateResult.failure};
        if (stopToken.stop_requested()) return {false, L"Instalação cancelada.", 0};
        std::vector<std::filesystem::path> expectedMods;
        for (const auto& file : files) expectedMods.push_back(file.destination);
        removeObsoleteMods(gamePath / L"mods", expectedMods,stopToken);
        if(stopToken.stop_requested()) return {false,L"Instalação cancelada.",0};
        report(InstallationStage::Mods,InstallationActivity::Publishing,L"Mods e configurações sincronizados.",
            privateResult.allAvailable ? InstallationStepState::Available : InstallationStepState::Complete);
        if(stopToken.stop_requested()) return {false,L"Instalação cancelada.",0};
        report(InstallationStage::Validation,InstallationActivity::Publishing,L"Preparando metadados finais...");

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
        if (!classpath) return {false, L"Não foi possível salvar o classpath do jogo.", 0,InstallationFailure::Disk};

        report(InstallationStage::Validation,InstallationActivity::Verifying,L"Conferindo a instalação final...");
        return {true, L"Instalação concluída e verificada.", files.size()};
    }
    catch (const std::exception& error)
    {
        return {false, L"Falha durante a instalação: " + utf8ToWide(error.what()), 0,InstallationFailure::Disk};
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
    return LauncherSettingsStore::effectiveGameDirectory(settings_);
}
