#include "services/MinecraftLauncherService.h"
#include "config/NicknameStore.h"
#include "services/JavaRuntimeService.h"
#include "services/JavaRuntimePackage.h"
#include "services/MinecraftLaunchCommandBuilder.h"
#include "services/ModCleanup.h"
#include "services/PortableJavaInstaller.h"
#include "utils/JsonValue.h"
#include "utils/UrlEncoding.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace
{
    bool expectInvalidNickname(std::wstring_view nickname)
    {
        MinecraftLauncherService service;
        const auto result = service.launch(nickname);
        if (result.success || result.message.find(L"nickname") == std::wstring::npos)
        {
            std::wcerr << L"Invalid nickname accepted: " << nickname << L'\n';
            return false;
        }
        return true;
    }
}

int main(int argumentCount, char* arguments[])
{
    // This optional mode lets you validate the installed instance manually without
    // making the user's environment a dependency of the automated tests.
    if (argumentCount >= 2 && std::string_view(arguments[1]) == "--validate-installed")
    {
        MinecraftLauncherService service;
        const auto result = service.validateInstallation();
        std::wcout << result.message << L'\n';
        return result.success ? 0 : 10;
    }

    // Manual diagnostic used to confirm the Forge process stays
    // alive. The nickname is deliberately ASCII, as Minecraft requires.
    if (argumentCount >= 3 && std::string_view(arguments[1]) == "--launch-installed")
    {
        const std::string nicknameText(arguments[2]);
        const std::wstring nickname(nicknameText.begin(), nicknameText.end());
        MinecraftLauncherService service;
        const auto result = service.launch(nickname);
        std::wcout << result.message << L" PID=" << result.processId << L'\n';
        if (!result.success) return 11;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (!service.isGameRunning())
        {
            std::wcout << L"Process exited with code " << service.processExitCode() << L".\n";
            return 12;
        }
        return 0;
    }

    // These cases exercise validation before any access to Java or the game.
    if (!expectInvalidNickname(L"ab") ||
        !expectInvalidNickname(L"nickname_com_mais_de_16") ||
        !expectInvalidNickname(L"nome inválido"))
    {
        return 1;
    }

    MinecraftLauncherService service;
    const auto expectedSuffix = std::filesystem::path(L".eternal-launcher") / L"minecraft";
    const auto gameDirectoryText = service.gameDirectory().wstring();
    const auto suffixText = expectedSuffix.wstring();
    if (!gameDirectoryText.ends_with(suffixText))
    {
        std::wcerr << L"Unexpected isolated directory: " << gameDirectoryText << L'\n';
        return 2;
    }

    JavaRuntimeService runtimeService;
    const auto runtimeSuffix = std::filesystem::path(L".eternal-launcher") / L"runtime";
    if (!runtimeService.runtimeDirectory().wstring().ends_with(runtimeSuffix.wstring()))
    {
        std::wcerr << L"Unexpected isolated Java directory.\n";
        return 3;
    }

    const auto runtimeFixture = std::filesystem::temp_directory_path() /
        (L"eternal-java-test-" + std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(runtimeFixture / L"bin");
    std::ofstream(runtimeFixture / L"release") << "JAVA_VERSION=\"17.0.1\"\n";
    std::ofstream(runtimeFixture / L"bin" / L"javaw.exe").put('w');
    const bool acceptedWithoutJavaExe = PortableJavaInstaller::isJava17Runtime(runtimeFixture);
    std::ofstream(runtimeFixture / L"bin" / L"java.exe").put('j');
    const bool acceptedCompleteRuntime = PortableJavaInstaller::isJava17Runtime(runtimeFixture);
    std::filesystem::remove_all(runtimeFixture);
    if (acceptedWithoutJavaExe || !acceptedCompleteRuntime)
    {
        std::wcerr << L"Portable Java validation did not check java.exe.\n";
        return 20;
    }

    // Uses an isolated temporary folder to check local persistence.
    const auto settingsDirectory = std::filesystem::temp_directory_path() /
        (L"eternal-nickname-test-" + std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    // Invalid nicknames must not replace the previous valid preference.
    if (!NicknameStore::save(settingsDirectory, L"Amigo_123") ||
        NicknameStore::load(settingsDirectory) != L"Amigo_123" ||
        NicknameStore::save(settingsDirectory, L"nome inválido") ||
        NicknameStore::load(settingsDirectory) != L"Amigo_123")
    {
        std::wcerr << L"Nickname persistence failed.\n";
        return 15;
    }
    std::filesystem::remove(NicknameStore::filePath(settingsDirectory));
    std::filesystem::remove(settingsDirectory);

    // A mod removed from the manifest disappears from the instance on the next Play.
    const auto modsDirectory = std::filesystem::temp_directory_path() /
        (L"eternal-mod-cleanup-test-" + std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count())) / L"mods";
    std::filesystem::create_directories(modsDirectory);
    std::ofstream(modsDirectory / L"atual.jar").put('a');
    std::ofstream(modsDirectory / L"removido.jar").put('b');
    std::ofstream(modsDirectory / L"[1.20.1] Usuário.jar").put('u');
    std::ofstream(modsDirectory / L"download.jar.part").put('c');
    removeObsoleteMods(modsDirectory, {modsDirectory / L"atual.jar", modsDirectory / L"removido.jar"});
    removeObsoleteMods(modsDirectory, {modsDirectory / L"atual.jar"});
    if (!std::filesystem::exists(modsDirectory / L"atual.jar") ||
        std::filesystem::exists(modsDirectory / L"removido.jar") ||
        !std::filesystem::exists(modsDirectory / L"[1.20.1] Usuário.jar") ||
        !std::filesystem::exists(modsDirectory / L"download.jar.part"))
    {
        std::wcerr << L"Mod cleanup did not respect the manifest.\n";
        return 18;
    }
    removeObsoleteMods(modsDirectory, {});
    if (std::filesystem::exists(modsDirectory / L"atual.jar") ||
        !std::filesystem::exists(modsDirectory / L"[1.20.1] Usuário.jar"))
    {
        std::wcerr << L"Empty mod manifest did not remove the managed JAR or deleted the user's mod.\n";
        return 19;
    }
    std::filesystem::remove_all(modsDirectory.parent_path());

    // Ensures the parser picks the portable ZIP, not the MSI installer that
    // appears before it in the real Adoptium response.
    constexpr std::string_view metadata = R"json(
        {"binary":{"installer":{"link":"https://example.test/java.msi"},
        "package":{"checksum":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "link":"https://example.test/java.zip","size":12345}}}
    )json";
    const auto package = JavaRuntimePackage::parse(metadata);
    if (!package || package->url != L"https://example.test/java.zip" || package->size != 12345)
    {
        std::wcerr << L"Java runtime metadata was not parsed correctly.\n";
        return 4;
    }

    // Exercita objetos, arrays e escapes usados pelos manifestos Mojang e privado.
    if (encodeRelativeUrlPath("mods/[1.20.1] SecurityCraft v1.10.1.jar") !=
            "mods/%5B1.20.1%5D%20SecurityCraft%20v1.10.1.jar" ||
        encodeRelativeUrlPath("mods/A+B.jar") != "mods/A%2BB.jar")
    {
        std::wcerr << L"Mod name was not URL-encoded correctly.\n";
        return 16;
    }

    const auto json = JsonValue::parse(R"json({"files":[{"path":"mods/teste.jar"}],"ok":true})json");
    const auto* files = json ? json->find("files") : nullptr;
    if (!json || files == nullptr || files->arrayValue() == nullptr ||
        files->arrayValue()->size() != 1)
    {
        std::wcerr << L"Valid manifest JSON was not parsed correctly.\n";
        return 5;
    }

    MinecraftLaunchCommandBuilder commandBuilder;
    const auto offlineCommand = commandBuilder.build(
        L"Amigo_123", L"C:\\java.exe", settingsDirectory);
    const MinecraftIdentity original{L"Amigo_123",
        L"0123456789abcdef0123456789abcdef", L"real-token", L"", L"client-id"};
    const auto onlineCommand = commandBuilder.build(original, L"C:\\java.exe", settingsDirectory);
    if (offlineCommand.find(L"--userType legacy") == std::wstring::npos ||
        offlineCommand.find(L"--accessToken 0") == std::wstring::npos ||
        onlineCommand.find(L"--userType msa") == std::wstring::npos ||
        onlineCommand.find(L"--accessToken real-token") == std::wstring::npos ||
        onlineCommand.find(original.uuid) == std::wstring::npos ||
        onlineCommand.find(L"--clientId client-id") == std::wstring::npos)
    {
        std::wcerr << L"Identity was not passed to Minecraft correctly.\n";
        return 17;
    }

    return 0;
}
