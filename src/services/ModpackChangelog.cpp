#include "services/ModpackChangelog.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <map>

namespace
{
    std::wstring lower(std::wstring_view value)
    {
        std::wstring result(value);
        std::transform(result.begin(), result.end(), result.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return result;
    }

    std::wstring_view stem(std::wstring_view file)
    {
        if (file.size() > 4 && lower(file.substr(file.size() - 4)) == L".jar") file.remove_suffix(4);
        return file;
    }

    bool separator(wchar_t c) { return c == L'-' || c == L'_' || c == L'+' || c == L' ' || c == L'.'; }
    bool digit(wchar_t c) { return c >= L'0' && c <= L'9'; }

    // Where the version starts: the first separator followed by a digit, "v1", or "mc1".
    std::size_t versionStart(std::wstring_view name)
    {
        for (std::size_t i = 1; i + 1 < name.size(); ++i) {
            if (!separator(name[i])) continue;
            const wchar_t next = std::towlower(name[i + 1]);
            if (digit(next)) return i;
            if (next == L'v' && i + 2 < name.size() && digit(name[i + 2])) return i;
            if (next == L'm' && i + 3 < name.size() && std::towlower(name[i + 2]) == L'c' && digit(name[i + 3])) return i;
        }
        return name.size();
    }

    std::string utf8(const std::wstring& value)
    {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(std::max(size, 0)), '\0');
        if (size > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring wide(const std::string& value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring result(static_cast<std::size_t>(std::max(size, 0)), L'\0');
        if (size > 0) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
        return result;
    }
}

std::wstring ModpackChangelog::modKey(std::wstring_view fileName)
{
    const auto name = stem(fileName);
    return lower(name.substr(0, versionStart(name)));
}

std::wstring ModpackChangelog::displayName(std::wstring_view fileName)
{
    const auto name = stem(fileName);
    std::wstring result(name.substr(0, versionStart(name)));
    bool wordStart = true;
    for (auto& c : result) {
        if (c == L'-' || c == L'_') { c = L' '; wordStart = true; continue; }
        if (wordStart) c = static_cast<wchar_t>(std::towupper(c));
        wordStart = false;
    }
    return result.empty() ? std::wstring(name) : result;
}

std::wstring ModpackChangelog::version(std::wstring_view fileName)
{
    const auto name = stem(fileName);
    const auto start = versionStart(name);
    if (start >= name.size()) return {};
    const std::wstring full(name.substr(start + 1));
    // Keep the mod's own version: drop the Minecraft version ("1.20.1", "mc1.20") and
    // loader names, which are the same for every mod of the pack.
    const auto minecraft = [](std::wstring token) {
        token = lower(token);
        if (token.rfind(L"mc", 0) == 0) token.erase(0, 2);
        if (token.size() < 4 || token.rfind(L"1.", 0) != 0) return false;
        const auto minor = token.substr(2, 2);
        if (minor.size() < 2 || !digit(minor[0]) || !digit(minor[1])) return false;
        const int value = (minor[0] - L'0') * 10 + (minor[1] - L'0');
        if (value < 12 || value > 30) return false;
        // "1.20" or "1.20.x" only.
        return token.size() == 4 || (token[4] == L'.' && token.find_first_not_of(L"0123456789", 5) == std::wstring::npos);
    };
    std::wstring result, token;
    const auto flush = [&] {
        const auto lowered = lower(token);
        if (!token.empty() && !minecraft(token) && lowered != L"forge" && lowered != L"neoforge" && lowered != L"fabric")
            result += (result.empty() ? L"" : L"-") + token;
        token.clear();
    };
    for (const wchar_t c : full) {
        if (c == L'-' || c == L'_' || c == L'+') flush();
        else token.push_back(c);
    }
    flush();
    return result.empty() ? full : result;
}

ModpackChanges ModpackChangelog::compare(const std::vector<std::wstring>& before, const std::vector<std::wstring>& after)
{
    std::map<std::wstring, std::wstring> old, current;
    for (const auto& file : before) old[modKey(file)] = file;
    for (const auto& file : after) current[modKey(file)] = file;
    ModpackChanges changes;
    for (const auto& [key, file] : current) {
        const auto found = old.find(key);
        if (found == old.end()) changes.added.push_back({displayName(file), version(file), {}});
        else if (lower(found->second) != lower(file)) changes.updated.push_back({displayName(file), version(file), version(found->second)});
    }
    for (const auto& [key, file] : old)
        if (!current.count(key)) changes.removed.push_back({displayName(file), version(file), {}});
    const auto byName = [](const ModChange& a, const ModChange& b) { return lower(a.name) < lower(b.name); };
    std::sort(changes.added.begin(), changes.added.end(), byName);
    std::sort(changes.removed.begin(), changes.removed.end(), byName);
    std::sort(changes.updated.begin(), changes.updated.end(), byName);
    return changes;
}

std::optional<std::vector<std::wstring>> ModpackChangelog::load(const std::filesystem::path& directory)
{
    std::ifstream input(directory / L"modpack-snapshot.txt", std::ios::binary);
    if (!input) return std::nullopt;
    std::vector<std::wstring> files;
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) files.push_back(wide(line));
    }
    return files;
}

bool ModpackChangelog::save(const std::filesystem::path& directory, const std::vector<std::wstring>& files)
{
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    std::ofstream output(directory / L"modpack-snapshot.txt", std::ios::binary | std::ios::trunc);
    for (const auto& file : files) output << utf8(file) << '\n';
    return static_cast<bool>(output);
}
