#include "config/GameServerSettings.h"

#include <windows.h>

#include <fstream>
#include <string>

namespace
{
    std::wstring fromUtf8(std::string_view text)
    {
        if (text.empty()) return {};
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (count <= 0) return {};
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), count);
        return result;
    }

    bool validName(std::wstring_view value)
    {
        return !value.empty() && value.size() <= 128 &&
            value.find_first_of(L"\r\n") == std::wstring_view::npos;
    }

    bool validAddress(std::wstring_view value)
    {
        return !value.empty() && value.size() <= 255 &&
            value.find_first_of(L"\r\n \t") == std::wstring_view::npos;
    }
}

std::filesystem::path GameServerSettingsStore::filePath(
    const std::filesystem::path& dataDirectory)
{
    return dataDirectory / L"servidor-jogo.ini";
}

bool GameServerSettingsStore::loadOrCreate(
    const std::filesystem::path& dataDirectory,
    GameServerSettings& settings,
    std::wstring& errorMessage)
{
    const auto path = filePath(dataDirectory);
    if (!std::filesystem::exists(path))
    {
        std::filesystem::create_directories(dataDirectory);
        std::ofstream output(path, std::ios::binary);
        // MOCK: o endereço inicial é apenas um exemplo editável pelo dono do grupo.
        output << "# Servidor Minecraft exibido na lista Multiplayer.\n"
               << "# Edite endereco para o IP ou dominio real, com porta se necessario.\n"
               << "nome=Eternal SMP\n"
               << "endereco=mc.seugrupo.com:25565\n";
        output.flush();
        if (!output)
        {
            errorMessage = L"Não foi possível criar servidor-jogo.ini.";
            return false;
        }
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        errorMessage = L"Não foi possível ler servidor-jogo.ini.";
        return false;
    }

    std::string line;
    bool hasName = false;
    bool hasAddress = false;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
        if (line.empty() || line.front() == '#') continue;
        const auto delimiter = line.find('=');
        if (delimiter == std::string::npos) continue;
        const auto key = line.substr(0, delimiter);
        const auto value = fromUtf8(std::string_view(line).substr(delimiter + 1));
        if (key == "nome")
        {
            settings.name = value;
            hasName = true;
        }
        else if (key == "endereco")
        {
            settings.address = value;
            hasAddress = true;
        }
    }

    if (!hasName || !hasAddress || !validName(settings.name) ||
        !validAddress(settings.address))
    {
        errorMessage = L"Confira nome e endereco em servidor-jogo.ini.";
        return false;
    }
    return true;
}
