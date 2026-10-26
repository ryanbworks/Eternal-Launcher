#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct ServerMod
{
    std::wstring name;
    std::uint64_t size = 0;
    bool installed = false;
};

struct UserMod
{
    std::wstring name;
    std::filesystem::path path;
    std::uint64_t size = 0;
    bool enabled = true;
};

struct ModInventorySnapshot
{
    std::vector<ServerMod> server;
    std::vector<UserMod> user;
    bool manifestKnown = false;
};

// Separates the mods the launcher manages (listed by the server manifest or recorded
// in .eternal-managed-mods) from the ones the player added to the instance.
class ModInventory
{
public:
    static ModInventorySnapshot scan(const std::filesystem::path& modsDirectory, std::wstring_view manifestJson);
    static std::vector<ServerMod> serverMods(std::wstring_view manifestJson, bool& known);
    // Enabling renames "x.jar.disabled" back to "x.jar"; disabling does the opposite.
    static bool setEnabled(const UserMod& mod, bool enabled, std::wstring& error);
    // Sends the file to the Recycle Bin so the player can undo it.
    static bool remove(const UserMod& mod, std::wstring& error);
    static bool add(const std::filesystem::path& modsDirectory, const std::filesystem::path& jar, std::wstring& error);
    static std::wstring formatSize(std::uint64_t bytes);
};
