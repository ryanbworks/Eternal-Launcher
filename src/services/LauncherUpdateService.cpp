#include "services/LauncherUpdateService.h"
#include "config/LauncherConfig.h"
#include "utils/FileHash.h"
#include "utils/JsonValue.h"
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <thread>
#include <cwctype>

namespace fs = std::filesystem;
namespace
{
    struct Handle {
        HANDLE value = nullptr;
        explicit Handle(HANDLE h = nullptr) : value(h) {}
        ~Handle() {if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);}
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
    };
    struct Mutex {
        Handle handle;
        bool owned = false;
        explicit Mutex(const std::wstring& name) : handle(CreateMutexW(nullptr, FALSE, name.c_str())) {
            if (handle.value) {const auto wait = WaitForSingleObject(handle.value, 0); owned = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;}
        }
        ~Mutex() {if (owned) ReleaseMutex(handle.value);}
    };
    std::string read(const fs::path& path, std::uintmax_t limit = 65536) {
        std::error_code ec;
        if (fs::file_size(path, ec) > limit || ec) return {};
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
    bool durableWrite(const fs::path& path, const std::string& data) {
        const auto temporary = fs::path(path.wstring() + L".tmp");
        {
            Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
            DWORD written = 0;
            if (file.value == INVALID_HANDLE_VALUE || !WriteFile(file.value, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) ||
                written != data.size() || !FlushFileBuffers(file.value)) return false;
        }
        return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    struct Journal {std::string phase, nonce, originalHash;};
    bool hexString(const std::string& text, std::size_t length) {
        return text.size() == length && std::all_of(text.begin(), text.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
    }
    std::optional<Journal> journal(const fs::path& dir) {
        const auto value = JsonValue::parse(read(dir / L"transaction.json"));
        if (!value) return {};
        const auto text = [&](const char* name) {
            const auto v = value->find(name); const auto p = v ? v->stringValue() : nullptr;
            return p ? *p : std::string{};
        };
        Journal j{text("phase"), text("nonce"), text("originalHash")};
        if (!hexString(j.nonce, 32) || !hexString(j.originalHash, 64)) return {};
        if (j.phase != "prepared" && j.phase != "applying" && j.phase != "applied" &&
            j.phase != "confirmed" && j.phase != "rolledback") return {};
        return j;
    }
    bool saveJournal(const fs::path& dir, Journal& value, const char* phase) {
        value.phase = phase;
        return durableWrite(dir / L"transaction.json", "{\"phase\":\"" + value.phase +
            "\",\"nonce\":\"" + value.nonce + "\",\"originalHash\":\"" + value.originalHash + "\"}\n");
    }
    std::wstring wide(std::string_view text) {return {text.begin(), text.end()};}
    std::wstring eventName(const Journal& j, bool ready) {
        return L"Local\\EternalLauncher." + std::wstring(ready ? L"Ready." : L"Handoff.") + wide(j.nonce);
    }
    bool samePath(const fs::path& a, const fs::path& b) {
        std::error_code ec1, ec2;
        const auto left = fs::weakly_canonical(a, ec1), right = fs::weakly_canonical(b, ec2);
        return !ec1 && !ec2 && _wcsicmp(left.c_str(), right.c_str()) == 0;
    }
    bool safeDirectory(const fs::path& dir) {
        const auto attributes = GetFileAttributesW(dir.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
    }
    bool plainFile(const fs::path& file) {
        const auto attributes = GetFileAttributesW(file.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY));
    }
    void cleanup(const fs::path& dir) {
        if (!safeDirectory(dir)) return;
        // Never recurse through user-controlled directories or remove unknown files.
        std::error_code ec;
        for (const auto name : {L"next.exe", L"failed.exe", L"previous.exe", L"manifest.json", L"manifest.json.tmp", L"transaction.json.tmp"})
            fs::remove(dir / name, ec);
        fs::remove(dir / L"helper.exe", ec);
        if (!fs::exists(dir / L"helper.exe", ec)) fs::remove(dir / L"transaction.json", ec);
        fs::remove(dir, ec);
    }
    std::wstring quote(const fs::path& path) {return L"\"" + path.wstring() + L"\"";}
    bool startProcess(const fs::path& executable, const std::wstring& arguments, PROCESS_INFORMATION& process) {
        auto command = quote(executable) + L" " + arguments;
        STARTUPINFOW startup{sizeof(STARTUPINFOW)};
        return CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
            executable.parent_path().c_str(), &startup, &process) != FALSE;
    }
    bool restore(const fs::path& target, const fs::path& dir, Journal& j) {
        if (plainFile(target) && FileHash::sha256(target) == j.originalHash)
            return saveJournal(dir, j, "rolledback");
        const auto previous = dir / L"previous.exe";
        if (!plainFile(previous) || FileHash::sha256(previous) != j.originalHash) return false;
        for (int attempt = 0; attempt < 40; ++attempt) {
            const bool exists = GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES;
            const bool restored = exists ? ReplaceFileW(target.c_str(), previous.c_str(), (dir / L"failed.exe").c_str(), 0, nullptr, nullptr) != FALSE
                : MoveFileExW(previous.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
            if (restored) return saveJournal(dir, j, "rolledback");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }
    void reportRecoveryFailure(const fs::path& dir) {
        const auto message = L"Não foi possível concluir a atualização. Sua cópia anterior está em:\n" +
            (dir / L"previous.exe").wstring() + L"\n\nFeche outras instâncias e restaure essa cópia como EternalLauncher.exe.";
        MessageBoxW(nullptr, message.c_str(), L"Atualização do Eternal Launcher", MB_OK | MB_ICONWARNING);
    }
    void reopenOld(const fs::path& target, const fs::path& dir, Journal& j) {
        if (!restore(target, dir, j)) {reportRecoveryFailure(dir); return;}
        PROCESS_INFORMATION old{};
        if (startProcess(target, L"--launcher-update-rollback " + wide(j.nonce), old)) {
            CloseHandle(old.hThread); CloseHandle(old.hProcess);
        } else reportRecoveryFailure(dir);
    }
    int apply(DWORD parentId, const fs::path& target, std::wstring_view nonce) {
        const auto dir = LauncherUpdateRuntime::transactionDirectory(target);
        if (!target.is_absolute() || !safeDirectory(dir) || !samePath(LauncherUpdateRuntime::executablePath(), dir / L"helper.exe")) return 2;
        Mutex update(LauncherUpdateRuntime::mutexName(target, true));
        if (!update.owned) return 2;
        auto j = journal(dir);
        if (!j || j->phase != "prepared" || wide(j->nonce) != nonce) return 2;
        Handle parent(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentId));
        std::array<wchar_t, 32768> parentPath{}; DWORD length = static_cast<DWORD>(parentPath.size());
        if (!parent.value || !QueryFullProcessImageNameW(parent.value, 0, parentPath.data(), &length) ||
            !samePath(parentPath.data(), target) || FileHash::sha256(target) != j->originalHash ||
            FileHash::sha256(dir / L"helper.exe") != j->originalHash) return 2;
        std::wstring error;
        const auto release = LauncherUpdateManifest::verify(read(dir / L"manifest.json"), error);
        if (!release || *LauncherUpdateManifest::parseVersion(release->version) <= *LauncherUpdateManifest::parseVersion(LauncherConfig::NumericVersion) ||
            !LauncherUpdateRuntime::validateExecutable(dir / L"next.exe", *release)) return 2;
        Handle ready(CreateEventW(nullptr, TRUE, FALSE, eventName(*j, true).c_str()));
        Handle handoff(OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName(*j, false).c_str()));
        if (!ready.value || !handoff.value || !SetEvent(handoff.value)) return 2;
        if (WaitForSingleObject(parent.value, 30000) != WAIT_OBJECT_0) return 2;
        // Check again after the old image is no longer mapped and before replacement.
        if (!LauncherUpdateRuntime::validateExecutable(dir / L"next.exe", *release) || !saveJournal(dir, *j, "applying")) {
            reopenOld(target, dir, *j); return 1;
        }
        if (!ReplaceFileW(target.c_str(), (dir / L"next.exe").c_str(), (dir / L"previous.exe").c_str(), 0, nullptr, nullptr) ||
            !saveJournal(dir, *j, "applied")) {
            reopenOld(target, dir, *j); return 1;
        }
        PROCESS_INFORMATION child{};
        if (!startProcess(target, L"--launcher-update-complete " + wide(j->nonce), child)) {
            reopenOld(target, dir, *j); return 1;
        }
        Handle childProcess(child.hProcess), childThread(child.hThread);
        const HANDLE waiters[] = {ready.value, childProcess.value};
        if (WaitForMultipleObjects(2, waiters, FALSE, 30000) != WAIT_OBJECT_0) {
            // Only this exact child process may be stopped during recovery.
            if (WaitForSingleObject(childProcess.value, 0) == WAIT_TIMEOUT) {
                if (!TerminateProcess(childProcess.value, 1) || WaitForSingleObject(childProcess.value, 5000) != WAIT_OBJECT_0) {
                    reportRecoveryFailure(dir); return 1;
                }
            }
            reopenOld(target, dir, *j); return 1;
        }
        if (!saveJournal(dir, *j, "confirmed")) return 1;
        cleanup(dir);
        return 0;
    }
}

fs::path LauncherUpdateRuntime::executablePath()
{
    std::array<wchar_t, 32768> path{};
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    return size && size < path.size() ? fs::path(path.data()) : fs::path{};
}
fs::path LauncherUpdateRuntime::transactionDirectory(const fs::path& executable)
{
    return executable.parent_path() / (executable.filename().wstring() + L".update");
}
std::wstring LauncherUpdateRuntime::mutexName(const fs::path& executable, bool update)
{
    auto path = fs::absolute(executable).lexically_normal().wstring();
    std::transform(path.begin(), path.end(), path.begin(), [](wchar_t c) {return static_cast<wchar_t>(towlower(c));});
    // A stable name per executable path; no file content is needed during replacement.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto c : path) {hash ^= static_cast<std::uint64_t>(c); hash *= 1099511628211ULL;}
    return L"Local\\EternalLauncher." + std::wstring(update ? L"Update." : L"App.") + std::to_wstring(hash);
}
bool LauncherUpdateRuntime::validateExecutable(const fs::path& file, const LauncherRelease& release)
{
    std::error_code ec;
    if (!plainFile(file) || fs::file_size(file, ec) != release.size || ec || FileHash::sha256(file) != release.sha256) return false;
    std::ifstream stream(file, std::ios::binary);
    IMAGE_DOS_HEADER dos{}; stream.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!stream || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos)) ||
        static_cast<std::uint64_t>(dos.e_lfanew) + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) > release.size) return false;
    stream.seekg(dos.e_lfanew);
    DWORD signature = 0; IMAGE_FILE_HEADER header{};
    stream.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    stream.read(reinterpret_cast<char*>(&header), sizeof(header));
#ifdef _WIN64
    constexpr WORD machine = IMAGE_FILE_MACHINE_AMD64;
#else
    constexpr WORD machine = IMAGE_FILE_MACHINE_I386;
#endif
    return stream && signature == IMAGE_NT_SIGNATURE && header.Machine == machine && (header.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) && !(header.Characteristics & IMAGE_FILE_DLL);
}
std::optional<int> LauncherUpdateRuntime::dispatch(int count, wchar_t** arguments)
{
    if (count < 2 || std::wstring_view(arguments[1]) != L"--apply-launcher-update") return {};
    if (count != 5) return 2;
    wchar_t* end = nullptr;
    const auto id = wcstoul(arguments[2], &end, 10);
    if (!id || !end || *end) return 2;
    try {return apply(id, arguments[3], arguments[4]);} catch (...) {return 2;}
}

bool LauncherUpdateRuntime::startup(int count, wchar_t** arguments, bool& skipCheck, std::wstring& warning)
{
    const auto target = executablePath(), dir = transactionDirectory(target);
    const bool completion = count == 3 && std::wstring_view(arguments[1]) == L"--launcher-update-complete";
    const bool rollback = count == 3 && std::wstring_view(arguments[1]) == L"--launcher-update-rollback";
    auto j = safeDirectory(dir) ? journal(dir) : std::nullopt;
    if ((completion || rollback) && j && wide(j->nonce) == arguments[2]) {
        std::wstring error;
        const auto release = LauncherUpdateManifest::verify(read(dir / L"manifest.json"), error);
        if (completion && j->phase == "applied" && release && release->version == LauncherConfig::NumericVersion && validateExecutable(target, *release)) {
            skipCheck = true; return true;
        }
        if (rollback && j->phase == "rolledback" && FileHash::sha256(target) == j->originalHash) {
            skipCheck = true; warning = L"A atualização falhou. A versão anterior foi restaurada."; return true;
        }
        return false;
    }
    Mutex updating(mutexName(target, true));
    if (!updating.owned) {warning = L"Uma atualização do launcher está em andamento."; return false;}
    if (!j) {
        std::error_code ec;
        if (fs::exists(dir / L"previous.exe", ec)) {
            skipCheck = true; warning = L"Há uma cópia de recuperação pendente. Baixe o launcher novamente.";
        } else if (fs::exists(dir, ec) && !ec) cleanup(dir);
        return true;
    }
    if (j->phase == "confirmed" || j->phase == "rolledback" || j->phase == "prepared") {
        if (j->phase != "confirmed") {
            skipCheck = true; warning = L"Uma atualização foi interrompida. Você pode continuar nesta versão.";
        }
        cleanup(dir); return true;
    }
    std::wstring error;
    const auto release = LauncherUpdateManifest::verify(read(dir / L"manifest.json"), error);
    if (release && release->version == LauncherConfig::NumericVersion && validateExecutable(target, *release)) {
        // Complete an interrupted transaction only after this process has created its window.
        skipCheck = true; return true;
    }
    if (FileHash::sha256(target) == j->originalHash) {
        skipCheck = true; warning = L"Uma atualização foi interrompida. Você pode continuar nesta versão.";
        cleanup(dir); return true;
    }
    warning = L"Há uma atualização incompleta. Baixe o launcher novamente.";
    skipCheck = true; return true;
}
void LauncherUpdateRuntime::windowReady(int count, wchar_t** arguments)
{
    const auto target = executablePath(), dir = transactionDirectory(target);
    if (!safeDirectory(dir)) return;
    auto j = journal(dir);
    if (!j) return;
    if (count == 3 && std::wstring_view(arguments[1]) == L"--launcher-update-complete" && wide(j->nonce) == arguments[2]) {
        Handle ready(OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName(*j, true).c_str()));
        if (ready.value) SetEvent(ready.value);
    } else if (j->phase == "applied" || j->phase == "applying") {
        std::wstring error;
        const auto release = LauncherUpdateManifest::verify(read(dir / L"manifest.json"), error);
        if (release && release->version == LauncherConfig::NumericVersion && validateExecutable(target, *release) &&
            saveJournal(dir, *j, "confirmed")) cleanup(dir);
    }
}

LauncherUpdateResult LauncherUpdateService::prepare(const fs::path& target, std::stop_token stop,
    const LauncherUpdateProgressCallback& progress, const ILauncherUpdateTransport& transport,
    bool confirmed) const
{
    LauncherUpdateResult result;
    const auto dir = LauncherUpdateRuntime::transactionDirectory(target);
    try {
        progress({LauncherUpdatePhase::Checking, 0});
        auto response = transport.manifest(stop);
        if (!response.success) {result.warning = response.error; return result;}
        std::wstring error;
        const auto release = LauncherUpdateManifest::verify(response.text, error);
        if (!release) {result.warning = error; return result;}
        if (*LauncherUpdateManifest::parseVersion(release->version) <= *LauncherUpdateManifest::parseVersion(LauncherConfig::NumericVersion)) return result;
        if (!confirmed) {
            result.updateAvailable = true;
            result.version = std::wstring(release->version.begin(), release->version.end());
            result.codename = std::wstring(release->codename.begin(), release->codename.end());
            return result;
        }
        if (stop.stop_requested()) return result;
        if (fs::exists(dir)) {result.warning = L"Há uma atualização pendente. Reabra o launcher para recuperar."; return result;}
        if (!fs::create_directory(dir) || !safeDirectory(dir)) throw std::runtime_error("staging");
        if (!durableWrite(dir / L"manifest.json", response.text)) throw std::runtime_error("manifest");
        Journal j;
        std::array<unsigned char, 16> nonce{};
        if (BCryptGenRandom(nullptr, nonce.data(), static_cast<ULONG>(nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) throw std::runtime_error("random");
        constexpr char hex[] = "0123456789abcdef";
        for (const auto byte : nonce) {j.nonce += hex[byte >> 4]; j.nonce += hex[byte & 15];}
        j.originalHash = FileHash::sha256(target);
        if (!hexString(j.originalHash, 64) || !saveJournal(dir, j, "prepared")) throw std::runtime_error("journal");
        progress({LauncherUpdatePhase::Downloading, 0});
        int previousPercent = -1;
        response = transport.download(release->path, dir / L"next.exe", release->size, stop, [&](std::uint64_t bytes) {
            const auto percent = static_cast<int>(std::min<std::uint64_t>(100, bytes * 100 / release->size));
            if (percent != previousPercent) {previousPercent = percent; progress({LauncherUpdatePhase::Downloading, percent});}
        });
        if (stop.stop_requested()) {cleanup(dir); return result;}
        if (!response.success) {result.warning = response.error; cleanup(dir); return result;}
        if (!LauncherUpdateRuntime::validateExecutable(dir / L"next.exe", *release)) {
            result.warning = L"O arquivo da atualização não passou na verificação."; cleanup(dir); return result;
        }
        { Handle staged(CreateFileW((dir / L"next.exe").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
          if (staged.value == INVALID_HANDLE_VALUE || !FlushFileBuffers(staged.value)) throw std::runtime_error("flush"); }
        progress({LauncherUpdatePhase::Applying, 100});
        if (!CopyFileW(target.c_str(), (dir / L"helper.exe").c_str(), TRUE)) throw std::runtime_error("copy helper");
        Handle handoff(CreateEventW(nullptr, TRUE, FALSE, eventName(j, false).c_str()));
        if (!handoff.value) throw std::runtime_error("handoff event");
        PROCESS_INFORMATION helper{};
        const auto args = L"--apply-launcher-update " + std::to_wstring(GetCurrentProcessId()) + L" " + quote(target) + L" " + wide(j.nonce);
        if (!startProcess(dir / L"helper.exe", args, helper)) throw std::runtime_error("start helper");
        Handle helperProcess(helper.hProcess), helperThread(helper.hThread);
        const HANDLE waiters[] = {handoff.value, helperProcess.value};
        const auto wait = WaitForMultipleObjects(2, waiters, FALSE, 10000);
        if (wait != WAIT_OBJECT_0 || stop.stop_requested()) {
            if (WaitForSingleObject(helperProcess.value, 0) == WAIT_TIMEOUT) {
                TerminateProcess(helperProcess.value, 1); WaitForSingleObject(helperProcess.value, 5000);
            }
            result.warning = L"Não foi possível preparar a troca do launcher."; cleanup(dir); return result;
        }
        progress({LauncherUpdatePhase::Restarting, 100});
        result.restart = true;
    } catch (...) {
        result.warning = L"Não foi possível gravar a atualização. Baixe o launcher pelo link abaixo.";
        // Preserve any backup from an interrupted apply; cleanup only our prepared download.
        const auto j = journal(dir);
        if (!j || j->phase == "prepared") cleanup(dir);
    }
    return result;
}
