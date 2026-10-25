#include "services/MinecraftLauncherService.h"

#include "config/LauncherConfig.h"
#include "services/MinecraftLaunchCommandBuilder.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <string>

namespace
{
    std::wstring readEnvironmentVariable(const wchar_t* name)
    {
        wchar_t* value = nullptr;
        std::size_t length = 0;
        if (_wdupenv_s(&value, &length, name) != 0 || value == nullptr)
        {
            return {};
        }

        std::wstring result(value);
        std::free(value);
        return result;
    }

    bool isJava17Runtime(const std::filesystem::path& javaExecutable)
    {
        const auto releaseFile = javaExecutable.parent_path().parent_path() / L"release";
        std::ifstream input(releaseFile);
        if (!input)
        {
            return false;
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (line.starts_with("JAVA_VERSION=\"17\"") ||
                line.starts_with("JAVA_VERSION=\"17."))
            {
                return true;
            }
        }
        return false;
    }

    bool isValidNickname(std::wstring_view nickname)
    {
        if (nickname.size() < 3 || nickname.size() > 16)
        {
            return false;
        }

        return std::all_of(nickname.begin(), nickname.end(), [](wchar_t character)
        {
            return (character >= L'a' && character <= L'z') ||
                   (character >= L'A' && character <= L'Z') ||
                   (character >= L'0' && character <= L'9') ||
                   character == L'_';
        });
    }

    std::wstring windowsErrorMessage(DWORD errorCode)
    {
        wchar_t* buffer = nullptr;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<wchar_t*>(&buffer),
            0,
            nullptr);

        std::wstring message = L"Erro do Windows " + std::to_wstring(errorCode);
        if (buffer != nullptr)
        {
            message += L": ";
            message += buffer;
            LocalFree(buffer);
        }
        return message;
    }
}

MinecraftLauncherService::MinecraftLauncherService(LauncherSettings settings) : settings_(std::move(settings)) {}

MinecraftLauncherService::~MinecraftLauncherService()
{
    if (processHandle_ != nullptr)
    {
        CloseHandle(processHandle_);
    }
}

MinecraftLaunchResult MinecraftLauncherService::validateInstallation() const
{
    std::wstring error;
    if (!LauncherSettingsStore::validate(settings_,error,false) ||
        (!settings_.javaExecutable.empty() && !LauncherSettingsStore::isJava17Executable(settings_.javaExecutable)))
        return {false,error.empty() ? L"O Java personalizado não está disponível. Confira as configurações." : error,
                0,MinecraftLaunchIssue::InvalidConfiguration};
    const auto javaPath = findJavaExecutable();
    if (javaPath.empty())
    {
        return {false,
                L"Java 17 não encontrado em %APPDATA%\\.eternal-launcher\\runtime.",
                0,
                MinecraftLaunchIssue::JavaMissing};
    }

    const auto gamePath = gameDirectory();
    const auto versionName = std::wstring(LauncherConfig::VersionProfile);
    const auto profile = gamePath / L"versions" / versionName / (versionName + L".json");
    const auto clientJar = gamePath / L"versions" / LauncherConfig::MinecraftVersion /
                           (std::wstring(LauncherConfig::MinecraftVersion) + L".jar");
    const auto assetIndex = gamePath / L"assets" / L"indexes" /
                            (std::wstring(LauncherConfig::AssetIndex) + L".json");
    const auto libraries = gamePath / L"libraries";
    const auto natives = gamePath / L"natives";

    // This list detects interrupted installations before the command is built.
    // Repair is delegated to GameInstallationService through the interface.
    const std::array<std::filesystem::path, 19> requiredPaths = {
        profile,
        clientJar,
        assetIndex,
        gamePath / L"eternal-classpath.txt",
        libraries / L"cpw/mods/bootstraplauncher/1.1.2/bootstraplauncher-1.1.2.jar",
        libraries / L"cpw/mods/securejarhandler/2.1.10/securejarhandler-2.1.10.jar",
        libraries / L"org/ow2/asm/asm-commons/9.8/asm-commons-9.8.jar",
        libraries / L"org/ow2/asm/asm-util/9.8/asm-util-9.8.jar",
        libraries / L"org/ow2/asm/asm-analysis/9.8/asm-analysis-9.8.jar",
        libraries / L"org/ow2/asm/asm-tree/9.8/asm-tree-9.8.jar",
        libraries / L"org/ow2/asm/asm/9.8/asm-9.8.jar",
        libraries / L"net/minecraftforge/JarJarFileSystems/0.3.19/JarJarFileSystems-0.3.19.jar",
        libraries / L"net/minecraftforge/fmlloader/1.20.1-47.4.10/fmlloader-1.20.1-47.4.10.jar",
        libraries / L"net/minecraftforge/fmlearlydisplay/1.20.1-47.4.10/fmlearlydisplay-1.20.1-47.4.10.jar",
        libraries / L"net/minecraftforge/fmlcore/1.20.1-47.4.10/fmlcore-1.20.1-47.4.10.jar",
        libraries / L"net/minecraftforge/forge/1.20.1-47.4.10/forge-1.20.1-47.4.10-universal.jar",
        libraries / L"net/minecraftforge/javafmllanguage/1.20.1-47.4.10/javafmllanguage-1.20.1-47.4.10.jar",
        libraries / L"net/minecraftforge/lowcodelanguage/1.20.1-47.4.10/lowcodelanguage-1.20.1-47.4.10.jar",
        libraries / L"net/minecraftforge/mclanguage/1.20.1-47.4.10/mclanguage-1.20.1-47.4.10.jar"
    };

    for (const auto& requiredPath : requiredPaths)
    {
        if (!std::filesystem::is_regular_file(requiredPath))
        {
            return {false,
                    L"A instalação Minecraft/Forge está incompleta. Arquivo ausente:\n" +
                        requiredPath.wstring(),
                    0,
                    MinecraftLaunchIssue::InstallationIncomplete};
        }
    }

    bool hasNativeLibrary = false;
    if (std::filesystem::is_directory(natives))
    {
        for (const auto& entry : std::filesystem::directory_iterator(natives))
        {
            if (entry.is_regular_file() && entry.path().extension() == L".dll")
            {
                hasNativeLibrary = true;
                break;
            }
        }
    }
    if (!hasNativeLibrary)
    {
        return {false,
                L"As bibliotecas nativas do Minecraft não foram preparadas em:\n" +
                    natives.wstring(),
                0,
                MinecraftLaunchIssue::NativeLibrariesMissing};
    }

    return {true, L"Minecraft 1.20.1 e Forge 47.4.10 estão preparados.", 0};
}

MinecraftLaunchResult MinecraftLauncherService::launch(std::wstring_view nickname)
{
    return launch(MinecraftIdentity{std::wstring(nickname)});
}

MinecraftLaunchResult MinecraftLauncherService::launch(const MinecraftIdentity& identity)
{
    if (isGameRunning())
    {
        return {false, L"O Minecraft já está em execução.", processId_,
                MinecraftLaunchIssue::GameAlreadyRunning};
    }

    if (processHandle_ != nullptr)
    {
        CloseHandle(processHandle_);
        processHandle_ = nullptr;
        processId_ = 0;
    }

    if (!isValidNickname(identity.name))
    {
        return {false,
                L"O nickname deve ter de 3 a 16 caracteres e usar apenas letras, números ou underline.",
                0,
                MinecraftLaunchIssue::InvalidNickname};
    }

    const auto validation = validateInstallation();
    if (!validation.success)
    {
        return validation;
    }

    const auto gamePath = gameDirectory();
    const auto javaPath = findJavaExecutable();

    // The command receives the selected identity before Java starts.
    MinecraftLaunchCommandBuilder commandBuilder;
    if (identity.online() && (identity.uuid.size() != 32 || identity.clientId.empty()))
    {
        return {false, L"A sessão Microsoft está incompleta. Entre novamente.", 0,
                MinecraftLaunchIssue::InvalidNickname};
    }
    std::wstring commandLine = commandBuilder.build(identity, javaPath, gamePath, settings_);

    STARTUPINFO startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    std::filesystem::create_directories(gamePath / L"logs");
    const auto consoleLog = gamePath / L"logs" / L"launcher-console.log";
    const HANDLE logHandle = CreateFile(
        consoleLog.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    const HANDLE inputHandle = CreateFile(
        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logHandle != INVALID_HANDLE_VALUE && inputHandle != INVALID_HANDLE_VALUE)
    {
        // Java stays windowless, but stdout/stderr remain available for
        // diagnostics when Forge or a mod exits during startup.
        SetHandleInformation(logHandle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(inputHandle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdInput = inputHandle;
        startupInfo.hStdOutput = logHandle;
        startupInfo.hStdError = logHandle;
    }
    PROCESS_INFORMATION processInformation{};
    const BOOL created = CreateProcess(
        javaPath.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        logHandle != INVALID_HANDLE_VALUE && inputHandle != INVALID_HANDLE_VALUE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
        nullptr,
        gamePath.c_str(),
        &startupInfo,
        &processInformation);

    if (logHandle != INVALID_HANDLE_VALUE) CloseHandle(logHandle);
    if (inputHandle != INVALID_HANDLE_VALUE) CloseHandle(inputHandle);

    if (!created)
    {
        return {false,
                L"Não foi possível iniciar o Java. " + windowsErrorMessage(GetLastError()),
                0,
                MinecraftLaunchIssue::ProcessCreationFailed};
    }

    CloseHandle(processInformation.hThread);
    processHandle_ = processInformation.hProcess;
    processId_ = processInformation.dwProcessId;
    return {true, L"Minecraft iniciado com o perfil Forge configurado.", processId_};
}

bool MinecraftLauncherService::isGameRunning() const
{
    if (processHandle_ == nullptr)
    {
        return false;
    }

    DWORD exitCode = 0;
    return GetExitCodeProcess(processHandle_, &exitCode) && exitCode == STILL_ACTIVE;
}

unsigned long MinecraftLauncherService::processExitCode() const
{
    DWORD exitCode = ERROR_INVALID_HANDLE;
    return processHandle_ != nullptr && GetExitCodeProcess(processHandle_, &exitCode)
        ? exitCode
        : ERROR_INVALID_HANDLE;
}

std::filesystem::path MinecraftLauncherService::gameDirectory() const
{
    return LauncherSettingsStore::effectiveGameDirectory(settings_);
}

std::filesystem::path MinecraftLauncherService::resolveDataDirectory() const
{
    const auto appData = readEnvironmentVariable(L"APPDATA");
    if (appData.empty())
    {
        return std::filesystem::current_path() / LauncherConfig::DataDirectoryName;
    }
    return std::filesystem::path(appData) / LauncherConfig::DataDirectoryName;
}

std::filesystem::path MinecraftLauncherService::findJavaExecutable() const
{
    if (!settings_.javaExecutable.empty())
        return LauncherSettingsStore::isJava17Executable(settings_.javaExecutable) ? settings_.javaExecutable : std::filesystem::path{};
    const auto bundledJava = resolveDataDirectory() / L"runtime" / L"bin" / L"java.exe";
    if (std::filesystem::is_regular_file(bundledJava) && isJava17Runtime(bundledJava))
    {
        return bundledJava;
    }

    const auto javaHome = readEnvironmentVariable(L"JAVA_HOME");
    if (!javaHome.empty())
    {
        const auto configuredJava = std::filesystem::path(javaHome) / L"bin" / L"java.exe";
        if (std::filesystem::is_regular_file(configuredJava) && isJava17Runtime(configuredJava))
        {
            return configuredJava;
        }
    }

    std::array<wchar_t, MAX_PATH> searchResult{};
    const DWORD length = SearchPath(
        nullptr, L"java.exe", nullptr, static_cast<DWORD>(searchResult.size()),
        searchResult.data(), nullptr);
    if (length > 0 && length < searchResult.size() &&
        isJava17Runtime(std::filesystem::path(searchResult.data())))
    {
        return std::filesystem::path(searchResult.data());
    }

    return {};
}
