#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// One mod in a changelog: a readable name and the version taken from its file name.
struct ModChange
{
    std::wstring name;
    std::wstring version;          // version after the change ("" when unknown)
    std::wstring previousVersion;  // updates only
};

struct ModpackChanges
{
    std::vector<ModChange> added;
    std::vector<ModChange> removed;
    std::vector<ModChange> updated;
    bool empty() const { return added.empty() && removed.empty() && updated.empty(); }
};

// What changed in the server's mod list since the player last saw it. The list the
// player acknowledged is kept in modpack-snapshot.txt in the data folder.
namespace ModpackChangelog
{
    // "create-1.20.1-0.5.1.f.jar" -> "create": the file name without its version, so
    // a new version of the same mod is an update and not a removal plus an addition.
    std::wstring modKey(std::wstring_view fileName);
    // "sophisticated_backpacks" -> "Sophisticated Backpacks".
    std::wstring displayName(std::wstring_view fileName);
    // "create-1.20.1-0.5.1.f.jar" -> "1.20.1-0.5.1.f".
    std::wstring version(std::wstring_view fileName);
    ModpackChanges compare(const std::vector<std::wstring>& before, const std::vector<std::wstring>& after);

    std::optional<std::vector<std::wstring>> load(const std::filesystem::path& directory);
    bool save(const std::filesystem::path& directory, const std::vector<std::wstring>& files);
}
