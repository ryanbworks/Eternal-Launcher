#include "services/HttpFileService.h"

#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <string>
#include <string_view>
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

        [[nodiscard]] HINTERNET get() const { return handle_; }
        [[nodiscard]] explicit operator bool() const { return handle_ != nullptr; }

    private:
        HINTERNET handle_;
    };

    std::wstring describeWindowsError(std::wstring_view context)
    {
        const DWORD errorCode = GetLastError();
        wchar_t* systemMessage = nullptr;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<wchar_t*>(&systemMessage),
            0,
            nullptr);

        std::wstring message(context);
        message += L" (erro " + std::to_wstring(errorCode) + L")";
        if (systemMessage != nullptr)
        {
            message += L": ";
            message += systemMessage;
            LocalFree(systemMessage);
        }
        return message;
    }

    std::wstring utf8ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int requiredSize = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        if (requiredSize <= 0)
        {
            return {};
        }

        std::wstring converted(static_cast<std::size_t>(requiredSize), L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            converted.data(), requiredSize);
        return converted;
    }

    std::wstring combineUrlPath(std::wstring basePath, std::wstring_view relativePath)
    {
        if (basePath.empty())
        {
            basePath = L"/";
        }
        if (!basePath.ends_with(L'/'))
        {
            basePath += L'/';
        }

        const std::size_t offset = relativePath.starts_with(L'/') ? 1U : 0U;
        basePath.append(relativePath.substr(offset));
        return basePath;
    }
}

HttpFileService::HttpFileService(std::wstring baseUrl)
    : baseUrl_(std::move(baseUrl))
{
}

FileServerManifestResponse HttpFileService::fetchManifest()
{
    FileServerManifestResponse result;
    const auto startedAt = std::chrono::steady_clock::now();

    URL_COMPONENTS urlParts{};
    urlParts.dwStructSize = sizeof(urlParts);
    urlParts.dwHostNameLength = static_cast<DWORD>(-1);
    urlParts.dwUrlPathLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(baseUrl_.c_str(), 0, 0, &urlParts))
    {
        result.errorMessage = describeWindowsError(L"URL do servidor HTTP inválida");
        return result;
    }

    const std::wstring host(urlParts.lpszHostName, urlParts.dwHostNameLength);
    const std::wstring basePath(urlParts.lpszUrlPath, urlParts.dwUrlPathLength);
    const std::wstring manifestPath = combineUrlPath(basePath, L"manifest.json");
    const bool secure = urlParts.nScheme == INTERNET_SCHEME_HTTPS;

    InternetHandle session(WinHttpOpen(
        L"EternalLauncher/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0));
    if (!session)
    {
        result.errorMessage = describeWindowsError(L"Não foi possível iniciar o cliente HTTP");
        return result;
    }

    // Tempos curtos impedem que uma indisponibilidade de rede congele o estado visual.
    WinHttpSetTimeouts(session.get(), 3000, 3000, 3000, 5000);

    InternetHandle connection(WinHttpConnect(
        session.get(), host.c_str(), urlParts.nPort, 0));
    if (!connection)
    {
        result.errorMessage = describeWindowsError(L"Não foi possível conectar ao servidor HTTP");
        return result;
    }

    InternetHandle request(WinHttpOpenRequest(
        connection.get(),
        L"GET",
        manifestPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        secure ? WINHTTP_FLAG_SECURE : 0));
    if (!request)
    {
        result.errorMessage = describeWindowsError(L"Não foi possível preparar a requisição HTTP");
        return result;
    }

    if (!WinHttpSendRequest(
            request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
    {
        result.errorMessage = describeWindowsError(L"Falha ao receber o manifesto HTTP");
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        request.get(),
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX);
    result.httpStatus = statusCode;

    std::string responseBytes;
    while (true)
    {
        DWORD availableBytes = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &availableBytes))
        {
            result.errorMessage = describeWindowsError(L"Falha ao ler o manifesto HTTP");
            return result;
        }
        if (availableBytes == 0)
        {
            break;
        }

        std::vector<char> buffer(availableBytes);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), availableBytes, &bytesRead))
        {
            result.errorMessage = describeWindowsError(L"Falha ao baixar o manifesto HTTP");
            return result;
        }
        responseBytes.append(buffer.data(), bytesRead);
    }

    result.content = utf8ToWide(responseBytes);
    result.elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - startedAt)
                                     .count();

    if (statusCode < 200 || statusCode >= 300)
    {
        result.errorMessage = L"O servidor respondeu com HTTP " + std::to_wstring(statusCode) + L".";
        return result;
    }
    if (result.content.empty())
    {
        result.errorMessage = L"O servidor retornou um manifesto vazio ou inválido.";
        return result;
    }

    result.success = true;
    return result;
}
