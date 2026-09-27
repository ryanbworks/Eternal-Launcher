#include "services/MinecraftLauncherService.h"

#include "config/LauncherConfig.h"

#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr wchar_t PathSeparator = L';';

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
            if (line.starts_with("JAVA_VERSION=\"17") ||
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

    std::string wideToUtf8(std::wstring_view value)
    {
        if (value.empty())
        {
            return {};
        }

        const int required = WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);
        std::string converted(static_cast<std::size_t>(required), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            converted.data(), required, nullptr, nullptr);
        return converted;
    }

    std::wstring createOfflineUuid(std::wstring_view nickname)
    {
        const std::string source = "OfflinePlayer:" + wideToUtf8(nickname);
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0;
        DWORD resultSize = 0;
        std::array<unsigned char, 16> digest{};

        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                              &resultSize, 0) < 0)
        {
            if (algorithm != nullptr)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
            }
            return L"00000000000030008000000000000000";
        }

        std::vector<unsigned char> hashObject(objectSize);
        const bool success =
            BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize,
                             nullptr, 0, 0) >= 0 &&
            BCryptHashData(hash,
                           reinterpret_cast<PUCHAR>(const_cast<char*>(source.data())),
                           static_cast<ULONG>(source.size()), 0) >= 0 &&
            BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;

        if (hash != nullptr)
        {
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);

        if (!success)
        {
            return L"00000000000030008000000000000000";
        }

        // UUID v3 compatível com o formato OfflinePlayer utilizado por servidores offline.
        digest[6] = static_cast<unsigned char>((digest[6] & 0x0F) | 0x30);
        digest[8] = static_cast<unsigned char>((digest[8] & 0x3F) | 0x80);

        std::wostringstream formatted;
        formatted << std::hex << std::setfill(L'0');
        for (const unsigned char byte : digest)
        {
            formatted << std::setw(2) << static_cast<int>(byte);
        }
        return formatted.str();
    }

    std::wstring quoteArgument(std::wstring_view argument)
    {
        if (argument.empty())
        {
            return L"\"\"";
        }
        if (argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
        {
            return std::wstring(argument);
        }

        std::wstring quoted = L"\"";
        std::size_t backslashes = 0;
        for (const wchar_t character : argument)
        {
            if (character == L'\\')
            {
                ++backslashes;
                continue;
            }

            if (character == L'\"')
            {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted += L'\"';
            }
            else
            {
                quoted.append(backslashes, L'\\');
                quoted += character;
            }
            backslashes = 0;
        }

        quoted.append(backslashes * 2, L'\\');
        quoted += L'\"';
        return quoted;
    }

    std::wstring joinArguments(const std::vector<std::wstring>& arguments)
    {
        std::wstring commandLine;
        for (const auto& argument : arguments)
        {
            if (!commandLine.empty())
            {
                commandLine += L' ';
            }
            commandLine += quoteArgument(argument);
        }
        return commandLine;
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

MinecraftLauncherService::MinecraftLauncherService() = default;

MinecraftLauncherService::~MinecraftLauncherService()
{
    if (processHandle_ != nullptr)
    {
        CloseHandle(processHandle_);
    }
}

MinecraftLaunchResult MinecraftLauncherService::launch(std::wstring_view nickname)
{
    if (isGameRunning())
    {
        return {false, L"O Minecraft já está em execução.", processId_};
    }

    if (processHandle_ != nullptr)
    {
        CloseHandle(processHandle_);
        processHandle_ = nullptr;
        processId_ = 0;
    }

    const auto validation = validateInstallation(nickname);
    if (!validation.success)
    {
        return validation;
    }

    const auto gamePath = gameDirectory();
    const auto javaPath = findJavaExecutable();
    std::wstring commandLine = buildCommandLine(nickname, javaPath, gamePath);

    STARTUPINFO startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInformation{};
    const BOOL created = CreateProcess(
        javaPath.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP,
        nullptr,
        gamePath.c_str(),
        &startupInfo,
        &processInformation);

    if (!created)
    {
        return {false,
                L"Não foi possível iniciar o Java. " + windowsErrorMessage(GetLastError()),
                0};
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

std::filesystem::path MinecraftLauncherService::gameDirectory() const
{
    return resolveDataDirectory() / LauncherConfig::GameDirectoryName;
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
    const auto bundledJava = resolveDataDirectory() / L"runtime" / L"bin" / L"javaw.exe";
    if (std::filesystem::is_regular_file(bundledJava) && isJava17Runtime(bundledJava))
    {
        return bundledJava;
    }

    const auto javaHome = readEnvironmentVariable(L"JAVA_HOME");
    if (!javaHome.empty())
    {
        const auto configuredJava = std::filesystem::path(javaHome) / L"bin" / L"javaw.exe";
        if (std::filesystem::is_regular_file(configuredJava) && isJava17Runtime(configuredJava))
        {
            return configuredJava;
        }
    }

    std::array<wchar_t, MAX_PATH> searchResult{};
    const DWORD length = SearchPath(
        nullptr, L"javaw.exe", nullptr, static_cast<DWORD>(searchResult.size()),
        searchResult.data(), nullptr);
    if (length > 0 && length < searchResult.size() &&
        isJava17Runtime(std::filesystem::path(searchResult.data())))
    {
        return std::filesystem::path(searchResult.data());
    }

    return {};
}

MinecraftLaunchResult MinecraftLauncherService::validateInstallation(
    std::wstring_view nickname) const
{
    if (!isValidNickname(nickname))
    {
        return {false,
                L"O nickname deve ter de 3 a 16 caracteres e usar apenas letras, números ou underline.",
                0};
    }

    const auto javaPath = findJavaExecutable();
    if (javaPath.empty())
    {
        return {false,
                L"Java 17 não encontrado. Instale o runtime em %APPDATA%\\.eternal-launcher\\runtime.",
                0};
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

    const std::array<std::filesystem::path, 12> requiredPaths = {
        profile,
        clientJar,
        assetIndex,
        libraries / L"cpw/mods/bootstraplauncher/1.1.2/bootstraplauncher-1.1.2.jar",
        libraries / L"cpw/mods/securejarhandler/2.1.10/securejarhandler-2.1.10.jar",
        libraries / L"org/ow2/asm/asm-commons/9.8/asm-commons-9.8.jar",
        libraries / L"org/ow2/asm/asm-util/9.8/asm-util-9.8.jar",
        libraries / L"org/ow2/asm/asm-analysis/9.8/asm-analysis-9.8.jar",
        libraries / L"org/ow2/asm/asm-tree/9.8/asm-tree-9.8.jar",
        libraries / L"org/ow2/asm/asm/9.8/asm-9.8.jar",
        libraries / L"net/minecraftforge/JarJarFileSystems/0.3.19/JarJarFileSystems-0.3.19.jar",
        libraries / L"net/minecraftforge/fmlloader/1.20.1-47.4.10/fmlloader-1.20.1-47.4.10.jar"
    };

    for (const auto& requiredPath : requiredPaths)
    {
        if (!std::filesystem::is_regular_file(requiredPath))
        {
            return {false,
                    L"A instalação Minecraft/Forge está incompleta. Arquivo ausente:\n" +
                        requiredPath.wstring(),
                    0};
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
                0};
    }

    return {true, L"Instalação validada.", 0};
}

std::wstring MinecraftLauncherService::buildClasspath(
    const std::filesystem::path& gamePath) const
{
    std::vector<std::filesystem::path> jars;
    const auto libraries = gamePath / L"libraries";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(libraries))
    {
        if (entry.is_regular_file() && entry.path().extension() == L".jar")
        {
            jars.push_back(entry.path());
        }
    }

    jars.push_back(
        gamePath / L"versions" / LauncherConfig::MinecraftVersion /
        (std::wstring(LauncherConfig::MinecraftVersion) + L".jar"));
    std::sort(jars.begin(), jars.end());

    std::wstring classpath;
    for (const auto& jar : jars)
    {
        if (!classpath.empty())
        {
            classpath += PathSeparator;
        }
        classpath += jar.wstring();
    }
    return classpath;
}

std::wstring MinecraftLauncherService::buildCommandLine(
    std::wstring_view nickname,
    const std::filesystem::path& javaExecutable,
    const std::filesystem::path& gamePath) const
{
    const auto libraries = gamePath / L"libraries";
    const auto natives = gamePath / L"natives";
    const auto assets = gamePath / L"assets";
    const std::wstring versionName(LauncherConfig::VersionProfile);

    const std::array<std::filesystem::path, 7> moduleJars = {
        libraries / L"cpw/mods/bootstraplauncher/1.1.2/bootstraplauncher-1.1.2.jar",
        libraries / L"cpw/mods/securejarhandler/2.1.10/securejarhandler-2.1.10.jar",
        libraries / L"org/ow2/asm/asm-commons/9.8/asm-commons-9.8.jar",
        libraries / L"org/ow2/asm/asm-util/9.8/asm-util-9.8.jar",
        libraries / L"org/ow2/asm/asm-analysis/9.8/asm-analysis-9.8.jar",
        libraries / L"org/ow2/asm/asm-tree/9.8/asm-tree-9.8.jar",
        libraries / L"org/ow2/asm/asm/9.8/asm-9.8.jar"
    };

    std::wstring modulePath;
    for (const auto& jar : moduleJars)
    {
        if (!modulePath.empty())
        {
            modulePath += PathSeparator;
        }
        modulePath += jar.wstring();
    }
    modulePath += PathSeparator;
    modulePath += (libraries / L"net/minecraftforge/JarJarFileSystems/0.3.19/"
                   L"JarJarFileSystems-0.3.19.jar").wstring();

    std::vector<std::wstring> arguments = {
        javaExecutable.wstring(),
        L"-Xms" + std::to_wstring(LauncherConfig::MinimumMemoryMegabytes) + L"M",
        L"-Xmx" + std::to_wstring(LauncherConfig::MaximumMemoryMegabytes) + L"M",
        L"-Djava.library.path=" + natives.wstring(),
        L"-Djna.tmpdir=" + natives.wstring(),
        L"-Dorg.lwjgl.system.SharedLibraryExtractPath=" + natives.wstring(),
        L"-Dio.netty.native.workdir=" + natives.wstring(),
        L"-Dminecraft.launcher.brand=EternalLauncher",
        L"-Dminecraft.launcher.version=" + std::wstring(LauncherConfig::LauncherVersion),
        L"-Djava.net.preferIPv6Addresses=system",
        L"-DignoreList=bootstraplauncher,securejarhandler,asm-commons,asm-util,asm-analysis,"
            L"asm-tree,asm,JarJarFileSystems,client-extra,fmlcore,javafmllanguage,"
            L"lowcodelanguage,mclanguage,forge-," + versionName + L".jar",
        L"-DmergeModules=jna-5.10.0.jar,jna-platform-5.10.0.jar",
        L"-DlibraryDirectory=" + libraries.wstring(),
        L"-p",
        modulePath,
        L"--add-modules",
        L"ALL-MODULE-PATH",
        L"--add-opens",
        L"java.base/java.util.jar=cpw.mods.securejarhandler",
        L"--add-opens",
        L"java.base/java.lang.invoke=cpw.mods.securejarhandler",
        L"--add-exports",
        L"java.base/sun.security.util=cpw.mods.securejarhandler",
        L"--add-exports",
        L"jdk.naming.dns/com.sun.jndi.dns=java.naming",
        L"-cp",
        buildClasspath(gamePath),
        L"cpw.mods.bootstraplauncher.BootstrapLauncher",
        L"--username",
        std::wstring(nickname),
        L"--version",
        versionName,
        L"--gameDir",
        gamePath.wstring(),
        L"--assetsDir",
        assets.wstring(),
        L"--assetIndex",
        std::wstring(LauncherConfig::AssetIndex),
        L"--uuid",
        createOfflineUuid(nickname),
        L"--accessToken",
        L"0",
        L"--clientId",
        L"",
        L"--xuid",
        L"",
        L"--userType",
        L"legacy",
        L"--versionType",
        L"release",
        L"--launchTarget",
        L"forgeclient",
        L"--fml.forgeVersion",
        std::wstring(LauncherConfig::ForgeVersion),
        L"--fml.mcVersion",
        std::wstring(LauncherConfig::MinecraftVersion),
        L"--fml.forgeGroup",
        L"net.minecraftforge",
        L"--fml.mcpVersion",
        L"20230612.114412"
    };

    return joinArguments(arguments);
}
