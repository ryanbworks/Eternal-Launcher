#include "services/PortableJavaInstaller.h"

#include <windows.h>

#include <cstdlib>
#include <fstream>
#include <optional>
#include <string_view>

namespace
{
    std::wstring readEnvironmentVariable(const wchar_t* name)
    {
        wchar_t* value = nullptr;
        std::size_t length = 0;
        if (_wdupenv_s(&value, &length, name) != 0 || value == nullptr)
        {
            return {};
        }
        std::wstring result(value);
        std::free(value);
        return result;
    }

    std::wstring quoteArgument(std::wstring_view value)
    {
        return L"\"" + std::wstring(value) + L"\"";
    }

    bool extractArchive(const std::filesystem::path& archive,
                        const std::filesystem::path& destination,
                        std::stop_token stopToken)
    {
        const auto tarExecutable = std::filesystem::path(
            readEnvironmentVariable(L"SystemRoot")) / L"System32" / L"tar.exe";
        if (!std::filesystem::is_regular_file(tarExecutable))
        {
            return false;
        }

        std::wstring commandLine = quoteArgument(tarExecutable.wstring()) + L" -xf " +
                                   quoteArgument(archive.wstring()) + L" -C " +
                                   quoteArgument(destination.wstring());
        STARTUPINFO startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};
        if (!CreateProcess(
                tarExecutable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo))
        {
            return false;
        }

        CloseHandle(processInfo.hThread);
        while (WaitForSingleObject(processInfo.hProcess, 100) == WAIT_TIMEOUT)
        {
            if (stopToken.stop_requested())
            {
                TerminateProcess(processInfo.hProcess, ERROR_CANCELLED);
                WaitForSingleObject(processInfo.hProcess, INFINITE);
                break;
            }
        }

        DWORD exitCode = 1;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hProcess);
        return !stopToken.stop_requested() && exitCode == 0;
    }

    std::optional<std::filesystem::path> findExtractedRuntime(
        const std::filesystem::path& stagingDirectory)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(stagingDirectory))
        {
            if (entry.is_regular_file() && entry.path().filename() == L"javaw.exe" &&
                entry.path().parent_path().filename() == L"bin")
            {
                return entry.path().parent_path().parent_path();
            }
        }
        return std::nullopt;
    }
}

bool PortableJavaInstaller::isJava17Runtime(const std::filesystem::path& runtime)
{
    std::ifstream release(runtime / L"release");
    std::string line;
    while (std::getline(release, line))
    {
        if (line.starts_with("JAVA_VERSION=\"17"))
        {
            return std::filesystem::is_regular_file(runtime / L"bin" / L"javaw.exe");
        }
    }
    return false;
}

PortableJavaInstallResult PortableJavaInstaller::install(
    const std::filesystem::path& archive,
    const std::filesystem::path& dataDirectory,
    std::stop_token stopToken)
{
    const auto staging = dataDirectory / L"runtime-staging";
    const auto runtime = dataDirectory / L"runtime";
    const auto backup = dataDirectory / L"runtime-backup";
    std::error_code ignored;

    // Os nomes são fixos e filhos diretos da pasta privada do launcher.
    std::filesystem::remove_all(staging, ignored);
    std::filesystem::create_directories(staging);
    if (!extractArchive(archive, staging, stopToken))
    {
        std::filesystem::remove_all(staging, ignored);
        return {false, L"Não foi possível extrair o Java portátil."};
    }

    const auto extracted = findExtractedRuntime(staging);
    if (!extracted || !isJava17Runtime(*extracted))
    {
        std::filesystem::remove_all(staging, ignored);
        return {false, L"O pacote extraído não contém um Java 17 válido."};
    }

    std::filesystem::remove_all(backup, ignored);
    if (std::filesystem::exists(runtime))
    {
        std::filesystem::rename(runtime, backup);
    }

    try
    {
        std::filesystem::rename(*extracted, runtime);
    }
    catch (...)
    {
        if (std::filesystem::exists(backup) && !std::filesystem::exists(runtime))
        {
            std::filesystem::rename(backup, runtime);
        }
        std::filesystem::remove_all(staging, ignored);
        throw;
    }

    std::filesystem::remove_all(backup, ignored);
    std::filesystem::remove_all(staging, ignored);
    return {true, L"Java 17 portátil instalado com sucesso."};
}
