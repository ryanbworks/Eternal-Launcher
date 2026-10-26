#include "services/ModInventory.h"

#include "services/ModCleanup.h"
#include "utils/JsonValue.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <set>

namespace
{
    std::wstring lower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return value;
    }

    std::string utf8(std::wstring_view value)
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

    std::set<std::wstring> managedRecord(const std::filesystem::path& modsDirectory)
    {
        std::set<std::wstring> names;
        std::ifstream input(modsDirectory / ManagedModsRecordName, std::ios::binary);
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) names.insert(lower(wide(line)));
        }
        return names;
    }

    bool endsWith(const std::wstring& value, std::wstring_view suffix)
    {
        return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
}

std::vector<ServerMod> ModInventory::serverMods(std::wstring_view manifestJson, bool& known)
{
    std::vector<ServerMod> mods;
    known = false;
    if (manifestJson.empty()) return mods;
    const auto document = JsonValue::parse(utf8(manifestJson));
    const auto* files = document ? document->find("files") : nullptr;
    const auto* list = files ? files->arrayValue() : nullptr;
    if (!list) return mods;
    known = true;
    for (const auto& entry : *list) {
        const auto* path = entry.find("path");
        const auto* text = path ? path->stringValue() : nullptr;
        if (!text || text->rfind("mods/", 0) != 0) continue;
        auto name = wide(text->substr(5));
        if (name.empty() || name.find(L'/') != std::wstring::npos) continue;
        const auto* size = entry.find("size");
        mods.push_back({std::move(name), size ? static_cast<std::uint64_t>(std::max(0.0, size->numberValue())) : 0, false});
    }
    std::sort(mods.begin(), mods.end(), [](const ServerMod& a, const ServerMod& b) { return lower(a.name) < lower(b.name); });
    return mods;
}

ModInventorySnapshot ModInventory::scan(const std::filesystem::path& modsDirectory, std::wstring_view manifestJson)
{
    ModInventorySnapshot snapshot;
    snapshot.server = serverMods(manifestJson, snapshot.manifestKnown);
    std::set<std::wstring> managed = managedRecord(modsDirectory);
    for (const auto& mod : snapshot.server) managed.insert(lower(mod.name));

    std::error_code error;
    if (!std::filesystem::is_directory(modsDirectory, error)) return snapshot;
    std::set<std::wstring> present;
    for (const auto& entry : std::filesystem::directory_iterator(modsDirectory, error)) {
        if (entry.is_symlink(error) || !entry.is_regular_file(error)) continue;
        const auto name = entry.path().filename().wstring();
        const auto key = lower(name);
        bool enabled = true;
        std::wstring base = name;
        if (endsWith(key, L".jar.disabled")) {
            enabled = false;
            base = name.substr(0, name.size() - std::wstring_view(L".disabled").size());
        } else if (!endsWith(key, L".jar")) continue;
        if (enabled) present.insert(key);
        if (managed.contains(lower(base))) continue;
        snapshot.user.push_back({base, entry.path(), entry.file_size(error), enabled});
    }
    for (auto& mod : snapshot.server) mod.installed = present.contains(lower(mod.name));
    std::sort(snapshot.user.begin(), snapshot.user.end(), [](const UserMod& a, const UserMod& b) { return lower(a.name) < lower(b.name); });
    return snapshot;
}

bool ModInventory::setEnabled(const UserMod& mod, bool enabled, std::wstring& error)
{
    if (mod.enabled == enabled) return true;
    auto target = mod.path.parent_path() / mod.name;
    if (!enabled) target += L".disabled";
    std::error_code code;
    if (std::filesystem::exists(target, code)) {
        error = L"Já existe um arquivo com esse nome na pasta de mods.";
        return false;
    }
    std::filesystem::rename(mod.path, target, code);
    if (code) {
        error = L"Não foi possível renomear o mod. Feche o jogo e tente novamente.";
        return false;
    }
    return true;
}

bool ModInventory::remove(const UserMod& mod, std::wstring& error)
{
    std::wstring from = mod.path.wstring();
    from.push_back(L'\0'); // SHFileOperation expects a double-null-terminated list.
    SHFILEOPSTRUCTW operation{};
    operation.wFunc = FO_DELETE;
    operation.pFrom = from.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    if (SHFileOperationW(&operation) != 0 || operation.fAnyOperationsAborted) {
        error = L"Não foi possível mover o mod para a Lixeira.";
        return false;
    }
    return true;
}

bool ModInventory::add(const std::filesystem::path& modsDirectory, const std::filesystem::path& jar, std::wstring& error)
{
    if (!endsWith(lower(jar.filename().wstring()), L".jar")) {
        error = L"Escolha um arquivo .jar.";
        return false;
    }
    std::error_code code;
    std::filesystem::create_directories(modsDirectory, code);
    const auto target = modsDirectory / jar.filename();
    if (std::filesystem::exists(target, code)) {
        error = L"Esse mod já está na pasta de mods.";
        return false;
    }
    std::filesystem::copy_file(jar, target, code);
    if (code) {
        error = L"Não foi possível copiar o mod para a pasta da instância.";
        return false;
    }
    return true;
}

std::wstring ModInventory::formatSize(std::uint64_t bytes)
{
    wchar_t value[32]{};
    if (bytes >= 1024ULL * 1024)
        swprintf_s(value, L"%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else
        swprintf_s(value, L"%llu KB", static_cast<unsigned long long>((bytes + 1023) / 1024));
    std::wstring result = value;
    std::replace(result.begin(), result.end(), L'.', L',');
    return result;
}
