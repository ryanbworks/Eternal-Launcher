#pragma once

#include "services/VerifiedDownloader.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

// Remembers which files already matched their manifest hash, by size and last write
// time, so a Play with everything installed does not read gigabytes again. A file that
// changes on disk (size or time) is hashed again; Reparar clears the whole cache.
class VerifiedFileCache
{
public:
    static VerifiedFileCache& shared();

    // Loads the cache kept in `file`. Until a file is opened the cache answers nothing.
    void open(const std::filesystem::path& file);
    // True when `path` is the same file (size and write time) that matched `hash` before.
    [[nodiscard]] bool matches(const std::filesystem::path& path, FileHashAlgorithm algorithm, const std::string& hash);
    void remember(const std::filesystem::path& path, FileHashAlgorithm algorithm, const std::string& hash);
    void clear();
    // Writes the cache when it changed (atomically, through a temporary file).
    void save();

private:
    struct Entry
    {
        std::uint64_t size = 0;
        std::int64_t written = 0;
        FileHashAlgorithm algorithm = FileHashAlgorithm::Sha1;
        std::string hash;
    };
    static std::wstring key(const std::filesystem::path& path);
    static bool stat(const std::filesystem::path& path, std::uint64_t& size, std::int64_t& written);

    std::mutex mutex_;
    std::filesystem::path file_;
    std::map<std::wstring, Entry> entries_;
    bool dirty_ = false;
};
