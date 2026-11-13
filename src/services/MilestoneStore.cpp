#include "services/MilestoneStore.h"

#include <fstream>
#include <string>

std::wstring MilestoneStore::key(std::wstring name)
{
    // Nicknames are ASCII and case-insensitive.
    for (auto& c : name) if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    return name;
}

void MilestoneStore::load(const std::filesystem::path& directory)
{
    file_ = directory / L"milestones.txt";
    levels_.clear();
    std::ifstream input(file_, std::ios::binary);
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto tab = line.find('\t');
        if (tab == std::string::npos || tab == 0 || tab + 1 >= line.size()) continue;
        const int level = line[tab + 1] - '0';
        if (level < 1 || level > 9) continue;
        // Nicknames are ASCII, so a byte-wise widening is exact.
        levels_[key(std::wstring(line.begin(), line.begin() + static_cast<std::ptrdiff_t>(tab)))] = level;
    }
}

bool MilestoneStore::save() const
{
    if (file_.empty()) return false;
    std::error_code ignored;
    std::filesystem::create_directories(file_.parent_path(), ignored);
    std::ofstream output(file_, std::ios::binary | std::ios::trunc);
    for (const auto& [name, level] : levels_) {
        std::string ascii;
        for (const wchar_t c : name) ascii.push_back(c < 128 ? static_cast<char>(c) : '?');
        output << ascii << '\t' << level << '\n';
    }
    return static_cast<bool>(output);
}

int MilestoneStore::celebrated(const std::wstring& name) const
{
    const auto found = levels_.find(key(name));
    return found == levels_.end() ? -1 : found->second;
}

void MilestoneStore::setCelebrated(const std::wstring& name, int level)
{
    if (name.empty()) return;
    levels_[key(name)] = level;
}
