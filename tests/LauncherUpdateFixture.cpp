#include "services/LauncherUpdateService.h"
#include "config/LauncherConfig.h"
#include <fstream>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;
namespace {
    std::string read(const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }
    void write(const fs::path& path, const std::string& text) {std::ofstream(path, std::ios::binary) << text;}
    struct FixtureTransport : ILauncherUpdateTransport {
        fs::path dir;
        std::stop_source& source;
        FixtureTransport(fs::path directory, std::stop_source& stop) : dir(std::move(directory)), source(stop) {}
        LauncherUpdateResponse manifest(std::stop_token) const override {
            if (fs::exists(dir / L"offline")) return {false, {}, L"Servidor indisponível."};
            return {true, read(dir / L"input.json"), {}};
        }
        LauncherUpdateResponse download(std::string_view, const fs::path& destination, std::uint64_t size,
            std::stop_token, const LauncherUpdateDownloadProgress& progress) const override {
            if (fs::exists(dir / L"cancel")) {
                write(destination, "partial"); source.request_stop(); return {false, {}, L"Cancelado."};
            }
            fs::copy_file(dir / L"artifact.exe", destination);
            progress(size);
            return {true, {}, {}};
        }
    };
    struct Lock {
        HANDLE handle;
        bool owned;
        explicit Lock(const std::wstring& name) : handle(CreateMutexW(nullptr, FALSE, name.c_str())), owned(false) {
            if (handle) {const auto r = WaitForSingleObject(handle, 0); owned = r == WAIT_OBJECT_0 || r == WAIT_ABANDONED;}
        }
        ~Lock() {if(owned) ReleaseMutex(handle); if(handle) CloseHandle(handle);}
    };
}
int wmain(int count, wchar_t** args)
{
    if (const auto result = LauncherUpdateRuntime::dispatch(count, args)) return *result;
    const auto target = LauncherUpdateRuntime::executablePath(), dir = target.parent_path();
    if (count == 2 && std::wstring_view(args[1]) == L"--unit") {
        using LauncherUpdateManifest::parseVersion;
        if (!parseVersion("1.0.0") || !parseVersion("65535.65535.65535") ||
            !(*parseVersion("1.10.0") > *parseVersion("1.9.9"))) return 1;
        for (const auto invalid : {"", "1.0", "1.0.0-Aurora", "01.0.0", "1.0.0.1", "-1.0.0", "65536.0.0", "1..0", "1.0.0 "})
            if (parseVersion(invalid)) return 1;
        std::cout << "Numeric version checks passed\n"; return 0;
    }
    if (count == 3 && std::wstring_view(args[1]) == L"--verify") {
        std::wstring error;
        const auto result = LauncherUpdateManifest::verify(read(args[2]), error);
        if (result) {std::cout << result->version << '\n'; return 0;}
        return 1;
    }
    const bool holdUpdate = count == 2 && std::wstring_view(args[1]) == L"--hold-update";
    Lock lock(LauncherUpdateRuntime::mutexName(target, holdUpdate));
    if (!lock.owned) {write(dir / L"result.txt", "busy"); return 0;}
    if (holdUpdate) {write(dir / L"holding.txt", "ready"); Sleep(5000); return 0;}
    bool skip = false; std::wstring warning;
    if (!LauncherUpdateRuntime::startup(count, args, skip, warning)) {
        write(dir / L"result.txt", "busy"); return 0;
    }
    if (count == 2 && std::wstring_view(args[1]) == L"--run") {
        std::stop_source source;
        FixtureTransport transport(dir, source);
        const auto result = LauncherUpdateService{}.prepare(target, source.get_token(), [](const auto&) {}, transport);
        write(dir / L"parent.txt", result.restart ? "handoff" : warning.empty() && result.warning.empty() ? "unchanged" : "failed");
        if (!result.restart) write(dir / L"result.txt", source.stop_requested() ? "cancelled" : result.warning.empty() ? "unchanged" : "failed");
        return 0;
    }
    const bool complete = count == 3 && std::wstring_view(args[1]) == L"--launcher-update-complete";
    const bool rollback = count == 3 && std::wstring_view(args[1]) == L"--launcher-update-rollback";
    if (complete && fs::exists(dir / L"fail-child")) return 7;
    if (complete && fs::exists(dir / L"hang-child")) Sleep(45000);
    HWND window = CreateWindowExW(0, L"STATIC", L"Update test", WS_OVERLAPPED, 0,0,40,40,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if (!window) return 8;
    LauncherUpdateRuntime::windowReady(count, args);
    write(dir / L"result.txt", complete ? "updated" : rollback ? "rolledback" : skip ? "recovered" : "normal");
    Sleep(200);
    DestroyWindow(window);
    return 0;
}
