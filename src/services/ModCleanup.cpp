#include "services/ModCleanup.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

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

    std::set<std::wstring> loadManagedMods(const std::filesystem::path& record)
    {
        std::set<std::wstring> managed;
        std::ifstream input(record, std::ios::binary);
        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            const std::u8string utf8(line.begin(), line.end());
            managed.insert(lower(std::filesystem::path(utf8).wstring()));
        }
        return managed;
    }

    void saveManagedMods(const std::filesystem::path& record, const std::set<std::wstring>& managed)
    {
        auto temporary = record;
        temporary += L".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            for (const auto& name : managed)
            {
                const auto utf8 = std::filesystem::path(name).u8string();
                output.write(reinterpret_cast<const char*>(utf8.data()),
                             static_cast<std::streamsize>(utf8.size()));
                output.put('\n');
            }
            if (!output) return;
        }
        std::error_code error;
        std::filesystem::rename(temporary, record, error);
        if (error) std::filesystem::remove(temporary, error);
    }
}

void removeObsoleteMods(const std::filesystem::path& modsDirectory,
                        const std::vector<std::filesystem::path>& expectedFiles,std::stop_token stop)
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

    const auto record = modsDirectory / ManagedModsRecordName;
    const auto previouslyManaged = loadManagedMods(record);

    for (const auto& entry : std::filesystem::directory_iterator(modsDirectory))
    {
        if(stop.stop_requested()) return;
        if (entry.is_symlink() || !entry.is_regular_file() ||
            lower(entry.path().extension().wstring()) != L".jar") continue;

        const auto name = lower(entry.path().filename().wstring());
        if (previouslyManaged.contains(name) && !expected.contains(name))
        {
            std::filesystem::remove(entry.path());
        }
    }

    saveManagedMods(record, expected);
}
