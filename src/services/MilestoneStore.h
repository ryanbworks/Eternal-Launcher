#pragma once

#include <filesystem>
#include <map>
#include <string>

// The highest play-time level already celebrated for each nickname, kept in
// milestones.txt in the data folder so a level is only celebrated once.
class MilestoneStore
{
public:
    void load(const std::filesystem::path& directory);
    bool save() const;
    // -1 while this nickname has never been seen.
    int celebrated(const std::wstring& name) const;
    void setCelebrated(const std::wstring& name, int level);
private:
    static std::wstring key(std::wstring name);
    std::filesystem::path file_;
    std::map<std::wstring, int> levels_;
};
