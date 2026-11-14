#include "utils/ArchiveExtractor.h"
#include "utils/InstallationProcess.h"

#include <windows.h>

#include <cstdlib>
#include <string>

namespace
{
    std::wstring environmentVariable(const wchar_t* name)
    {
        wchar_t* value = nullptr;
        std::size_t length = 0;
        if (_wdupenv_s(&value, &length, name) != 0 || value == nullptr) return {};
        std::wstring result(value);
        std::free(value);
        return result;
    }

    std::wstring quote(std::wstring_view value)
    {
        return L"\"" + std::wstring(value) + L"\"";
    }
}

bool ArchiveExtractor::extract(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination,
    std::stop_token stopToken)
{
    return extractEntry(archive, {}, destination, stopToken);
}

bool ArchiveExtractor::extractEntry(
    const std::filesystem::path& archive,
    const std::wstring& entry,
    const std::filesystem::path& destination,
    std::stop_token stopToken)
{
    if(stopToken.stop_requested()) return false;
    const auto tar = std::filesystem::path(environmentVariable(L"SystemRoot")) /
                     L"System32" / L"tar.exe";
    if (!std::filesystem::is_regular_file(tar)) return false;

    std::filesystem::create_directories(destination);
    std::wstring command = quote(tar.wstring()) + L" -xf " + quote(archive.wstring()) +
                           L" -C " + quote(destination.wstring());
    if (!entry.empty()) command += L" " + quote(entry);
    STARTUPINFO startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcess(tar.c_str(), command.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return false;

    CloseHandle(process.hThread);
    const auto result=waitForInstallationProcess(process.hProcess,stopToken);
    CloseHandle(process.hProcess);
    return result.success;
}

namespace
{
    constexpr const wchar_t* NativesMarker = L".eternal-natives";

    // One line per archive: its path, size and last write time.
    std::wstring nativesSignature(const std::vector<std::filesystem::path>& archives)
    {
        std::wstring signature;
        for (const auto& archive : archives) {
            WIN32_FILE_ATTRIBUTE_DATA data{};
            if (!GetFileAttributesExW(archive.c_str(), GetFileExInfoStandard, &data)) return {};
            signature += archive.wstring() + L"|" + std::to_wstring(data.nFileSizeLow) + L"|" + std::to_wstring(data.nFileSizeHigh) + L"|" +
                std::to_wstring(data.ftLastWriteTime.dwLowDateTime) + L"|" + std::to_wstring(data.ftLastWriteTime.dwHighDateTime) + L"\n";
        }
        return signature;
    }

    std::wstring readMarker(const std::filesystem::path& file)
    {
        const HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return {};
        std::wstring content(32768, L'\0');
        DWORD read = 0;
        const bool ok = ReadFile(handle, content.data(), static_cast<DWORD>(content.size() * sizeof(wchar_t)), &read, nullptr) != FALSE;
        CloseHandle(handle);
        if (!ok) return {};
        content.resize(read / sizeof(wchar_t));
        return content;
    }
}

bool ArchiveExtractor::prepareNatives(const std::vector<std::filesystem::path>& archives, const std::filesystem::path& directory,
                                      const std::filesystem::path& required, std::stop_token stopToken, bool& reused)
{
    reused = false;
    std::error_code ignored;
    const auto signature = nativesSignature(archives);
    if (!signature.empty() && readMarker(directory / NativesMarker) == signature &&
        (required.empty() || std::filesystem::is_regular_file(directory / required, ignored))) {
        reused = true;
        return true;
    }
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    for (const auto& archive : archives)
        if (!extract(archive, directory, stopToken)) return false;
    return true;
}

void ArchiveExtractor::rememberNatives(const std::vector<std::filesystem::path>& archives, const std::filesystem::path& directory)
{
    const auto signature = nativesSignature(archives);
    if (signature.empty()) return;
    const HANDLE handle = CreateFileW((directory / NativesMarker).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(handle, signature.data(), static_cast<DWORD>(signature.size() * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(handle);
}
