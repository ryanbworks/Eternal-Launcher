#include "services/MinecraftLauncherService.h"

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

    return 0;
}
