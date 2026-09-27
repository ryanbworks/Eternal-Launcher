#include "services/MinecraftLauncherService.h"
#include "config/NicknameStore.h"
#include "services/JavaRuntimeService.h"
#include "services/JavaRuntimePackage.h"
#include "utils/JsonValue.h"

#include <chrono>
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
            std::wcerr << L"Nickname inválido aceito: " << nickname << L'\n';
            return false;
        }
        return true;
    }
}

int main(int argumentCount, char* arguments[])
{
    // Este modo opcional permite validar manualmente a instância instalada sem
    // tornar o ambiente do usuário uma dependência dos testes automatizados.
    if (argumentCount >= 2 && std::string_view(arguments[1]) == "--validate-installed")
    {
        MinecraftLauncherService service;
        const auto result = service.validateInstallation();
        std::wcout << result.message << L'\n';
        return result.success ? 0 : 10;
    }

    // Diagnóstico manual usado para confirmar que o processo Forge permanece
    // ativo. O nickname é deliberadamente ASCII, como exige o Minecraft.
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
            std::wcout << L"Processo encerrado com código " << service.processExitCode() << L".\n";
            return 12;
        }
        return 0;
    }

    // Estes casos exercitam a validação antes de qualquer acesso ao Java ou ao jogo.
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
        std::wcerr << L"Diretório isolado inesperado: " << gameDirectoryText << L'\n';
        return 2;
    }

    JavaRuntimeService runtimeService;
    const auto runtimeSuffix = std::filesystem::path(L".eternal-launcher") / L"runtime";
    if (!runtimeService.runtimeDirectory().wstring().ends_with(runtimeSuffix.wstring()))
    {
        std::wcerr << L"Diretório isolado do Java inesperado.\n";
        return 3;
    }

    // Usa uma pasta temporária isolada para conferir a persistência local.
    const auto settingsDirectory = std::filesystem::temp_directory_path() /
        (L"eternal-nickname-test-" + std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    // Nicknames inválidos não devem substituir a preferência válida anterior.
    if (!NicknameStore::save(settingsDirectory, L"Amigo_123") ||
        NicknameStore::load(settingsDirectory) != L"Amigo_123" ||
        NicknameStore::save(settingsDirectory, L"nome inválido") ||
        NicknameStore::load(settingsDirectory) != L"Amigo_123")
    {
        std::wcerr << L"Persistência do nickname falhou.\n";
        return 15;
    }
    std::filesystem::remove(NicknameStore::filePath(settingsDirectory));
    std::filesystem::remove(settingsDirectory);

    // Garante que o parser escolha o ZIP portátil, não o instalador MSI que
    // aparece antes dele na resposta real do Adoptium.
    constexpr std::string_view metadata = R"json(
        {"binary":{"installer":{"link":"https://example.test/java.msi"},
        "package":{"checksum":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "link":"https://example.test/java.zip","size":12345}}}
    )json";
    const auto package = JavaRuntimePackage::parse(metadata);
    if (!package || package->url != L"https://example.test/java.zip" || package->size != 12345)
    {
        std::wcerr << L"Metadados do runtime Java não foram interpretados corretamente.\n";
        return 4;
    }

    // Exercita objetos, arrays e escapes usados pelos manifestos Mojang e privado.
    const auto json = JsonValue::parse(R"json({"files":[{"path":"mods/teste.jar"}],"ok":true})json");
    const auto* files = json ? json->find("files") : nullptr;
    if (!json || files == nullptr || files->arrayValue() == nullptr ||
        files->arrayValue()->size() != 1)
    {
        std::wcerr << L"JSON de manifesto válido não foi interpretado corretamente.\n";
        return 5;
    }

    return 0;
}
