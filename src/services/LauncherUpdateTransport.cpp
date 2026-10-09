#include "services/LauncherUpdateTransport.h"
#include "services/LauncherUpdateManifest.h"
#include "config/LauncherConfig.h"
#include <windows.h>
#include <winhttp.h>
#include <array>
#include <atomic>
#include <fstream>

namespace
{
    struct InternetHandle {
        HINTERNET value = nullptr;
        ~InternetHandle() {if (value) WinHttpCloseHandle(value);}
    };
    // The request owns the callback context until HANDLE_CLOSING, including cancellation.
    struct Request {
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HANDLE closed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HINTERNET handle = nullptr;
        std::atomic<DWORD> status{0}, length{0}, error{0};
        ~Request() {
            if (handle) {
                WinHttpCloseHandle(handle);
                WaitForSingleObject(closed, INFINITE);
            }
            if (event) CloseHandle(event);
            if (closed) CloseHandle(closed);
        }
        static void CALLBACK callback(HINTERNET, DWORD_PTR context, DWORD code, LPVOID data, DWORD bytes) {
            const auto self = reinterpret_cast<Request*>(context);
            if (!self) return;
            if (code == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) { SetEvent(self->closed); return; }
            if (code == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE) self->length = *static_cast<DWORD*>(data);
            else if (code == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) self->length = bytes;
            else if (code == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
                self->error = static_cast<WINHTTP_ASYNC_RESULT*>(data)->dwError;
            else if (code != WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE && code != WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE) return;
            self->status = code;
            SetEvent(self->event);
        }
        bool await(DWORD expected, std::stop_token stop, ULONGLONG deadline) {
            while (!stop.stop_requested() && GetTickCount64() < deadline) {
                if (status == expected) return true;
                if (error) return false;
                WaitForSingleObject(event, 25);
            }
            return false;
        }
        void reset() {status = 0; length = 0; ResetEvent(event);}
    };
    bool pending(BOOL success) {return success || GetLastError() == ERROR_IO_PENDING;}
    LauncherUpdateResponse fetch(std::wstring_view path, const std::filesystem::path& destination,
        std::uint64_t limit, bool metadata, std::stop_token stop, const LauncherUpdateDownloadProgress& progress)
    {
        LauncherUpdateResponse result;
        result.error = L"Não foi possível baixar a atualização. Tente na próxima abertura.";
        const auto deadline = GetTickCount64() + 10000;
        InternetHandle session{WinHttpOpen(ETERNAL_USER_AGENT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC)};
        if (!session.value) return result;
        WinHttpSetTimeouts(session.value, 5000, 5000, 10000, metadata ? 10000 : 30000);
        InternetHandle connection{WinHttpConnect(session.value, L"eternalfast.aplot.cloud", INTERNET_DEFAULT_HTTPS_PORT, 0)};
        if (!connection.value) return result;
        const std::wstring ownedPath(path);
        std::array<char, 32768> buffer{}; // Outlives cancellation and the last asynchronous callback.
        Request request;
        if (!request.event || !request.closed) return result;
        request.handle = WinHttpOpenRequest(connection.value, L"GET", ownedPath.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request.handle) return result;
        // No redirects: both metadata and binaries must stay at the pinned HTTPS origin.
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        DWORD_PTR context = reinterpret_cast<DWORD_PTR>(&request);
        if (WinHttpSetStatusCallback(request.handle, Request::callback,
                WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES, 0) == WINHTTP_INVALID_STATUS_CALLBACK ||
            !WinHttpSetOption(request.handle, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context))) {
            // No callback can safely acknowledge this handle in this failure branch.
            WinHttpSetStatusCallback(request.handle, nullptr, 0, 0);
            WinHttpCloseHandle(request.handle); request.handle = nullptr; return result;
        }
        if (!WinHttpSetOption(request.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect))) return result;
        constexpr wchar_t headers[] = L"Cache-Control: no-cache\r\nAccept-Encoding: identity\r\n";
        if (!pending(WinHttpSendRequest(request.handle, headers, static_cast<DWORD>(-1), nullptr, 0, 0, context)) ||
            !request.await(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, stop, deadline)) return result;
        request.reset();
        if (!pending(WinHttpReceiveResponse(request.handle, nullptr)) ||
            !request.await(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, stop, deadline)) return result;
        DWORD status = 0, statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &status, &statusSize, nullptr) || status != 200) {
            result.error = L"O servidor de atualização respondeu com HTTP " + std::to_wstring(status) + L".";
            return result;
        }
        std::ofstream output;
        if (!metadata) {
            output.open(destination, std::ios::binary | std::ios::trunc);
            if (!output) {result.error = L"Sem permissão para gravar a atualização nesta pasta."; return result;}
        }
        std::uint64_t total = 0;
        while (!stop.stop_requested()) {
            const auto readDeadline = metadata ? deadline : GetTickCount64() + 30000;
            request.reset();
            if (!pending(WinHttpReadData(request.handle, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr)) ||
                !request.await(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, stop, readDeadline)) return result;
            const DWORD bytes = request.length;
            if (!bytes) {
                if (!total || (!metadata && total != limit)) {result.error = L"O download da atualização está incompleto."; return result;}
                if (output.is_open()) {output.flush(); if (!output) return result;}
                result.success = true; result.error.clear(); return result;
            }
            if (total + bytes > limit) {result.error = L"A atualização excedeu o tamanho permitido."; return result;}
            total += bytes;
            if (metadata) result.text.append(buffer.data(), bytes);
            else {output.write(buffer.data(), bytes); if (!output) return result;}
            if (progress) progress(total);
        }
        result.error = L"Atualização cancelada.";
        return result;
    }
}

LauncherUpdateResponse LauncherUpdateTransport::manifest(std::stop_token stop) const
{
    return fetch(L"/launcher/latest.json", {}, LauncherUpdateManifest::MaximumManifestSize, true, stop, {});
}
LauncherUpdateResponse LauncherUpdateTransport::download(std::string_view path, const std::filesystem::path& destination,
    std::uint64_t size, std::stop_token stop, const LauncherUpdateDownloadProgress& progress) const
{
    if (!path.starts_with("/launcher/releases/") || size > LauncherUpdateManifest::MaximumExecutableSize)
        return {false, {}, L"Caminho de atualização inválido."};
    return fetch(std::wstring(path.begin(), path.end()), destination, size, false, stop, progress);
}
