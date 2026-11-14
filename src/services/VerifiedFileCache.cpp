#include "services/VerifiedFileCache.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>

namespace
{
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

VerifiedFileCache& VerifiedFileCache::shared()
{
    static VerifiedFileCache cache;
    return cache;
}

std::wstring VerifiedFileCache::key(const std::filesystem::path& path)
{
    // Windows paths are case-insensitive; one spelling per file.
    std::wstring text = path.lexically_normal().wstring();
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return text;
}

bool VerifiedFileCache::stat(const std::filesystem::path& path, std::uint64_t& size, std::int64_t& written)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
    size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    written = static_cast<std::int64_t>((static_cast<std::uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime);
    return true;
}

void VerifiedFileCache::open(const std::filesystem::path& file)
{
    std::scoped_lock lock(mutex_);
    file_ = file;
    entries_.clear();
    dirty_ = false;
    std::ifstream input(file, std::ios::binary);
    // One file per line: algorithm, size, write time, hash and path, separated by tabs.
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream fields(line);
        std::string algorithm, size, written, hash, path;
        if (!std::getline(fields, algorithm, '\t') || !std::getline(fields, size, '\t') || !std::getline(fields, written, '\t') ||
            !std::getline(fields, hash, '\t') || !std::getline(fields, path) || path.empty() || hash.empty())
            continue;
        try {
            Entry entry;
            entry.algorithm = algorithm == "sha256" ? FileHashAlgorithm::Sha256 : FileHashAlgorithm::Sha1;
            entry.size = std::stoull(size);
            entry.written = std::stoll(written);
            entry.hash = hash;
            entries_[key(wide(path))] = std::move(entry);
        } catch (const std::exception&) {
            // A damaged line only costs one hash.
        }
    }
}

bool VerifiedFileCache::matches(const std::filesystem::path& path, FileHashAlgorithm algorithm, const std::string& hash)
{
    std::uint64_t size = 0;
    std::int64_t written = 0;
    if (hash.empty() || !stat(path, size, written)) return false;
    std::scoped_lock lock(mutex_);
    if (file_.empty()) return false;
    const auto found = entries_.find(key(path));
    return found != entries_.end() && found->second.size == size && found->second.written == written &&
           found->second.algorithm == algorithm && found->second.hash == hash;
}

void VerifiedFileCache::remember(const std::filesystem::path& path, FileHashAlgorithm algorithm, const std::string& hash)
{
    std::uint64_t size = 0;
    std::int64_t written = 0;
    if (hash.empty() || !stat(path, size, written)) return;
    std::scoped_lock lock(mutex_);
    if (file_.empty()) return;
    entries_[key(path)] = {size, written, algorithm, hash};
    dirty_ = true;
}

void VerifiedFileCache::clear()
{
    std::scoped_lock lock(mutex_);
    if (entries_.empty()) return;
    entries_.clear();
    dirty_ = true;
}

void VerifiedFileCache::save()
{
    std::scoped_lock lock(mutex_);
    if (file_.empty() || !dirty_) return;
    std::error_code ignored;
    std::filesystem::create_directories(file_.parent_path(), ignored);
    auto partial = file_;
    partial += L".part";
    {
        std::ofstream output(partial, std::ios::binary | std::ios::trunc);
        for (const auto& [path, entry] : entries_)
            output << (entry.algorithm == FileHashAlgorithm::Sha256 ? "sha256" : "sha1") << '\t' << entry.size << '\t' << entry.written << '\t'
                   << entry.hash << '\t' << utf8(path) << '\n';
        if (!output) return;
    }
    if (MoveFileExW(partial.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) dirty_ = false;
}
