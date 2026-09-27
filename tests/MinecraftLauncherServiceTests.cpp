#include "services/MinecraftLauncherService.h"
#include "services/JavaRuntimeService.h"
#include "services/JavaRuntimePackage.h"

#include <iostream>

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

int main()
{
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

    return 0;
}
