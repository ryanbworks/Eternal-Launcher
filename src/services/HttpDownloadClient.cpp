#include "services/HttpDownloadClient.h"

#include <windows.h>
#include <winhttp.h>

#include <fstream>
#include <utility>
#include <vector>

namespace
{
    class InternetHandle
    {
    public:
        explicit InternetHandle(HINTERNET handle = nullptr) : handle_(handle) {}
        ~InternetHandle()
        {
            if (handle_ != nullptr)
            {
                WinHttpCloseHandle(handle_);
            }
        }

        InternetHandle(const InternetHandle&) = delete;
        InternetHandle& operator=(const InternetHandle&) = delete;
        InternetHandle(InternetHandle&& other) noexcept : handle_(other.handle_)
        {
            other.handle_ = nullptr;
        }
        InternetHandle& operator=(InternetHandle&& other) noexcept
        {
            if (this != &other)
            {
                if (handle_ != nullptr)
                {
                    WinHttpCloseHandle(handle_);
                }
                handle_ = other.handle_;
                other.handle_ = nullptr;
            }
            return *this;
        }

        [[nodiscard]] HINTERNET get() const { return handle_; }
        [[nodiscard]] explicit operator bool() const { return handle_ != nullptr; }

    private:
        HINTERNET handle_ = nullptr;
    };

    std::wstring describeWindowsError(std::wstring_view context)
    {
        const DWORD code = GetLastError();
        return std::wstring(context) + L" (erro " + std::to_wstring(code) + L").";
    }

    struct RequestHandles
    {
        InternetHandle session;
        InternetHandle connection;
        InternetHandle request;
        std::wstring error;
    };

    RequestHandles openGetRequest(std::wstring_view url)
    {
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = static_cast<DWORD>(-1);
        parts.dwUrlPathLength = static_cast<DWORD>(-1);
        parts.dwExtraInfoLength = static_cast<DWORD>(-1);

        const std::wstring ownedUrl(url);
        if (!WinHttpCrackUrl(ownedUrl.c_str(), 0, 0, &parts))
        {
            RequestHandles handles;
            handles.error = describeWindowsError(L"URL de download inválida");
            return handles;
        }

        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.dwExtraInfoLength > 0)
        {
            path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        }

        RequestHandles handles;
        handles.session = InternetHandle(WinHttpOpen(
            L"EternalLauncher/0.1",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0));
        if (!handles.session)
        {
            handles.error = describeWindowsError(L"Não foi possível iniciar o cliente HTTP");
            return handles;
        }

        // Downloads maiores podem demorar, mas cada operação individual possui timeout.
        WinHttpSetTimeouts(handles.session.get(), 5000, 5000, 10000, 30000);
        handles.connection = InternetHandle(
            WinHttpConnect(handles.session.get(), host.c_str(), parts.nPort, 0));
        if (!handles.connection)
        {
            handles.error = describeWindowsError(L"Não foi possível conectar ao servidor");
            return handles;
        }

        handles.request = InternetHandle(WinHttpOpenRequest(
            handles.connection.get(), L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
        if (!handles.request)
        {
            handles.error = describeWindowsError(L"Não foi possível preparar o download");
            return handles;
        }

        if (!WinHttpSendRequest(
                handles.request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(handles.request.get(), nullptr))
        {
            handles.error = describeWindowsError(L"Falha ao receber o download");
        }
        return handles;
    }

    DWORD queryStatus(HINTERNET request)
    {
        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(
            request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        return status;
    }

    std::uint64_t queryContentLength(HINTERNET request)
    {
        wchar_t value[64]{};
        DWORD size = sizeof(value);
        if (!WinHttpQueryHeaders(
                request, WINHTTP_QUERY_CONTENT_LENGTH,
                WINHTTP_HEADER_NAME_BY_INDEX, value, &size, WINHTTP_NO_HEADER_INDEX))
        {
            return 0;
        }
        return _wcstoui64(value, nullptr, 10);
    }
}

HttpDownloadResult HttpDownloadClient::getText(std::wstring_view url) const
{
    HttpDownloadResult result;
    auto handles = openGetRequest(url);
    if (!handles.error.empty())
    {
        result.errorMessage = std::move(handles.error);
        return result;
    }

    result.statusCode = queryStatus(handles.request.get());
    if (result.statusCode < 200 || result.statusCode >= 300)
    {
        result.errorMessage = L"O servidor respondeu com HTTP " +
                              std::to_wstring(result.statusCode) + L".";
        return result;
    }

    while (true)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(handles.request.get(), &available))
        {
            result.errorMessage = describeWindowsError(L"Falha ao ler os metadados");
            return result;
        }
        if (available == 0)
        {
            break;
        }

        std::vector<char> buffer(available);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(handles.request.get(), buffer.data(), available, &bytesRead))
        {
            result.errorMessage = describeWindowsError(L"Falha ao baixar os metadados");
            return result;
        }
        result.content.append(buffer.data(), bytesRead);
    }

    result.success = !result.content.empty();
    if (!result.success)
    {
        result.errorMessage = L"O servidor retornou metadados vazios.";
    }
    return result;
}

HttpDownloadResult HttpDownloadClient::downloadFile(
    std::wstring_view url,
    const std::filesystem::path& destination,
    std::stop_token stopToken,
    const HttpDownloadProgressCallback& progress) const
{
    HttpDownloadResult result;
    auto handles = openGetRequest(url);
    if (!handles.error.empty())
    {
        result.errorMessage = std::move(handles.error);
        return result;
    }

    result.statusCode = queryStatus(handles.request.get());
    if (result.statusCode < 200 || result.statusCode >= 300)
    {
        result.errorMessage = L"O servidor respondeu com HTTP " +
                              std::to_wstring(result.statusCode) + L".";
        return result;
    }

    std::filesystem::create_directories(destination.parent_path());
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        result.errorMessage = L"Não foi possível criar o arquivo temporário do Java.";
        return result;
    }

    const auto total = queryContentLength(handles.request.get());
    std::uint64_t received = 0;
    while (!stopToken.stop_requested())
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(handles.request.get(), &available))
        {
            result.errorMessage = describeWindowsError(L"Falha durante o download do Java");
            break;
        }
        if (available == 0)
        {
            result.success = true;
            break;
        }

        std::vector<char> buffer(available);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(handles.request.get(), buffer.data(), available, &bytesRead))
        {
            result.errorMessage = describeWindowsError(L"Falha ao receber o Java");
            break;
        }

        output.write(buffer.data(), bytesRead);
        if (!output)
        {
            result.errorMessage = L"Falha ao gravar o runtime Java no disco.";
            break;
        }
        received += bytesRead;
        if (progress)
        {
            progress(received, total);
        }
    }

    output.close();
    if (stopToken.stop_requested())
    {
        result.success = false;
        result.errorMessage = L"Download do Java cancelado.";
    }
    if (!result.success)
    {
        std::error_code ignored;
        std::filesystem::remove(destination, ignored);
    }
    return result;
}
