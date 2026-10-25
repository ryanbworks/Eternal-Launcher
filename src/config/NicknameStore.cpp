#include "config/NicknameStore.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <string>

bool NicknameStore::isValid(std::wstring_view nickname)
{
    // Same rules accepted by the field and by the game's offline launch.
    return nickname.size() >= 3 && nickname.size() <= 16 &&
        std::all_of(nickname.begin(), nickname.end(), [](wchar_t character)
        {
            return (character >= L'a' && character <= L'z') ||
                   (character >= L'A' && character <= L'Z') ||
                   (character >= L'0' && character <= L'9') || character == L'_';
        });
}

std::filesystem::path NicknameStore::filePath(
    const std::filesystem::path& dataDirectory)
{
    return dataDirectory / L"nickname.txt";
}

std::wstring NicknameStore::load(const std::filesystem::path& dataDirectory)
{
    const auto path = filePath(dataDirectory);
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > 32) return {};
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::string text;
    std::getline(input, text);
    if (!text.empty() && text.back() == '\r') text.pop_back();
    if (text.size() > 16) return {};
    const std::wstring nickname(text.begin(), text.end());
    return isValid(nickname) ? nickname : std::wstring{};
}

bool NicknameStore::save(
    const std::filesystem::path& dataDirectory,
    std::wstring_view nickname)
{
    if (!isValid(nickname)) return false;
    std::error_code error;
    std::filesystem::create_directories(dataDirectory, error);
    if (error) return false;

    // Write to a temporary file first so the last valid value is never lost.
    const auto destination = filePath(dataDirectory);
    const auto temporary = std::filesystem::path(destination.wstring() + L".tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        for (const auto character : nickname) output.put(static_cast<char>(character));
        output.put('\n');
        output.flush();
        if (!output) return false;
    }
    return MoveFileExW(temporary.c_str(), destination.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
