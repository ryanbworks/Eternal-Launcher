#include "services/ModCleanup.h"

#include <algorithm>
#include <cwctype>
#include <set>
#include <stdexcept>

namespace
{
    std::wstring lower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character)
        {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return value;
    }
}

void removeObsoleteMods(const std::filesystem::path& modsDirectory,
                        const std::vector<std::filesystem::path>& expectedFiles)
{
    if (!std::filesystem::exists(modsDirectory)) return;
    if (std::filesystem::is_symlink(modsDirectory) ||
        !std::filesystem::is_directory(modsDirectory))
    {
        throw std::runtime_error("A pasta de mods local não é um diretório seguro.");
    }

    std::set<std::wstring> expected;
    for (const auto& file : expectedFiles)
    {
        if (file.parent_path() == modsDirectory && lower(file.extension().wstring()) == L".jar")
        {
            expected.insert(lower(file.filename().wstring()));
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(modsDirectory))
    {
        if (entry.is_symlink() || !entry.is_regular_file() ||
            lower(entry.path().extension().wstring()) != L".jar") continue;

        if (!expected.contains(lower(entry.path().filename().wstring())))
        {
            std::filesystem::remove(entry.path());
        }
    }
}
