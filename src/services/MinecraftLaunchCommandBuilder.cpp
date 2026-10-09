#include "services/MinecraftLaunchCommandBuilder.h"

#include "config/LauncherConfig.h"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace
{
    constexpr wchar_t PathSeparator = L';';

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

    std::wstring utf8ToWide(std::string_view value)
    {
        if (value.empty()) return {};
        const int required = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        if (required <= 0) return {};
        std::wstring converted(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            converted.data(), required);
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
}

std::wstring MinecraftLaunchCommandBuilder::build(
    std::wstring_view nickname,
    const std::filesystem::path& javaExecutable,
    const std::filesystem::path& gamePath) const
{
    return build(MinecraftIdentity{std::wstring(nickname)}, javaExecutable, gamePath);
}

std::wstring MinecraftLaunchCommandBuilder::build(
    const MinecraftIdentity& identity,
    const std::filesystem::path& javaExecutable,
    const std::filesystem::path& gamePath) const
{
    return build(identity,javaExecutable,gamePath,LauncherSettings{});
}

std::wstring MinecraftLaunchCommandBuilder::build(
    const MinecraftIdentity& identity, const std::filesystem::path& javaExecutable,
    const std::filesystem::path& gamePath, const LauncherSettings& settings) const
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
        L"-Xms" + std::to_wstring(settings.minimumMemoryMegabytes) + L"M",
        L"-Xmx" + std::to_wstring(settings.maximumMemoryMegabytes) + L"M",
        L"-Djava.library.path=" + natives.wstring(),
        L"-Djna.tmpdir=" + natives.wstring(),
        L"-Dorg.lwjgl.system.SharedLibraryExtractPath=" + natives.wstring(),
        L"-Dio.netty.native.workdir=" + natives.wstring(),
        L"-Dlog4j.configurationFile=" +
            (assets / L"log_configs" / L"client-1.12.xml").wstring(),
        L"-Dminecraft.launcher.brand=EternalLauncher",
        L"-Dminecraft.launcher.version=" + std::wstring(LauncherConfig::LauncherVersion),
        L"-Djava.net.preferIPv6Addresses=system",
        L"-DignoreList=bootstraplauncher,securejarhandler,asm-commons,asm-util,asm-analysis,"
            L"asm-tree,asm,JarJarFileSystems,client-extra,fmlcore,javafmllanguage,"
            L"lowcodelanguage,mclanguage,forge-," +
            std::wstring(LauncherConfig::MinecraftVersion) + L".jar",
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
        identity.name,
        L"--version",
        versionName,
        L"--gameDir",
        gamePath.wstring(),
        L"--assetsDir",
        assets.wstring(),
        L"--assetIndex",
        std::wstring(LauncherConfig::AssetIndex),
        L"--uuid",
        identity.online() ? identity.uuid : createOfflineUuid(identity.name),
        L"--accessToken",
        identity.online() ? identity.accessToken : L"0",
        L"--clientId",
        identity.online() ? identity.clientId : L"",
        L"--xuid",
        identity.online() ? identity.xuid : L"",
        L"--userType",
        identity.online() ? L"msa" : L"legacy",
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

    arguments.insert(arguments.end(), {L"--width",std::to_wstring(settings.windowWidth),
                                      L"--height",std::to_wstring(settings.windowHeight)});
    if (settings.fullscreen) arguments.push_back(L"--fullscreen");
    return joinArguments(arguments);
}

std::wstring MinecraftLaunchCommandBuilder::buildClasspath(
    const std::filesystem::path& gamePath) const
{
    const auto controlledClasspath = gamePath / L"eternal-classpath.txt";
    std::ifstream manifest(controlledClasspath, std::ios::binary);
    if (manifest)
    {
        std::wstring classpath;
        std::string line;
        while (std::getline(manifest, line))
        {
            // O arquivo é gravado em modo texto no Windows; em leitura binária,
            // o retorno de carro do CRLF precisa ser removido do caminho Java.
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (!classpath.empty()) classpath += PathSeparator;
            classpath += utf8ToWide(line);
        }
        if (!classpath.empty()) return classpath;
    }

    // A validação impede a inicialização sem esse manifesto. Retornar vazio é
    // mais seguro que incluir JARs auxiliares deixados pelo instalador Forge.
    return {};
}
