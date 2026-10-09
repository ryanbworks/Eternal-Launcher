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
    if(stopToken.stop_requested()) return false;
    const auto tar = std::filesystem::path(environmentVariable(L"SystemRoot")) /
                     L"System32" / L"tar.exe";
    if (!std::filesystem::is_regular_file(tar)) return false;

    std::filesystem::create_directories(destination);
    std::wstring command = quote(tar.wstring()) + L" -xf " + quote(archive.wstring()) +
                           L" -C " + quote(destination.wstring());
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
