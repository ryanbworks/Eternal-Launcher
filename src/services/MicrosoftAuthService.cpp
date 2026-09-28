#include "services/MicrosoftAuthService.h"

#include "config/LauncherConfig.h"
#include "utils/JsonValue.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>
#include <thread>

namespace
{
    struct Handle
    {
        HINTERNET value = nullptr;
        ~Handle() { if (value) WinHttpCloseHandle(value); }
        Handle() = default;
        explicit Handle(HINTERNET handle) : value(handle) {}
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
    };

    struct Response
    {
        DWORD status = 0;
        std::string body;
    };

    std::wstring wide(std::string_view text)
    {
        if (text.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0) return {};
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), length);
        return result;
    }

    std::string narrow(std::wstring_view text)
    {
        if (text.empty()) return {};
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
            result.data(), length, nullptr, nullptr);
        return result;
    }

    std::string urlEncode(std::string_view value)
    {
        constexpr char digits[] = "0123456789ABCDEF";
        std::string result;
        for (const unsigned char c : value)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') result += c;
            else { result += '%'; result += digits[c >> 4]; result += digits[c & 15]; }
        }
        return result;
    }

    std::string jsonEscape(std::string_view value)
    {
        std::string result;
        for (const char c : value)
        {
            if (c == '"' || c == '\\') result += '\\';
            result += c;
        }
        return result;
    }

    std::optional<Response> request(std::wstring_view url, std::wstring_view method,
                                    std::wstring_view contentType, std::string_view body,
                                    std::wstring_view authorization = {})
    {
        const std::wstring ownedUrl(url);
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
        if (!WinHttpCrackUrl(ownedUrl.c_str(), 0, 0, &parts) ||
            parts.nScheme != INTERNET_SCHEME_HTTPS) return std::nullopt;
        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        Handle session(WinHttpOpen(L"EternalLauncher/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.value) return std::nullopt;
        WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 15000);
        Handle connection(WinHttpConnect(session.value, host.c_str(), parts.nPort, 0));
        if (!connection.value) return std::nullopt;
        Handle handle(WinHttpOpenRequest(connection.value, std::wstring(method).c_str(),
            path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE));
        if (!handle.value) return std::nullopt;
        std::wstring headers = L"Content-Type: " + std::wstring(contentType) + L"\r\n";
        if (!authorization.empty()) headers += L"Authorization: Bearer " +
            std::wstring(authorization) + L"\r\n";
        if (!WinHttpSendRequest(handle.value, headers.c_str(), static_cast<DWORD>(headers.size()),
            body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) ||
            !WinHttpReceiveResponse(handle.value, nullptr)) return std::nullopt;
        Response response;
        DWORD size = sizeof(response.status);
        if (!WinHttpQueryHeaders(handle.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &response.status, &size, WINHTTP_NO_HEADER_INDEX))
            return std::nullopt;
        while (response.body.size() < 1024 * 1024)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(handle.value, &available)) return std::nullopt;
            if (!available) return response;
            if (available > 1024 * 1024 - response.body.size()) return std::nullopt;
            std::string chunk(available, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(handle.value, chunk.data(), available, &read)) return std::nullopt;
            response.body.append(chunk.data(), read);
        }
        return std::nullopt;
    }

    std::string field(const JsonValue& json, std::string_view key)
    {
        const auto* value = json.find(key);
        const auto* text = value ? value->stringValue() : nullptr;
        return text ? *text : std::string{};
    }

    std::optional<JsonValue> parse(const std::optional<Response>& response)
    {
        return response ? JsonValue::parse(response->body) : std::nullopt;
    }

    std::wstring failed(std::wstring_view step, const std::optional<Response>& response)
    {
        if (!response) return L"Falha de conexão durante " + std::wstring(step) + L".";
        if (response->status == 403 && response->body.find("Invalid app registration") != std::string::npos)
            return L"O aplicativo Microsoft ainda não foi autorizado para Minecraft Services. "
                   L"Envie o Application (client) ID para análise no formulário indicado "
                   L"em https://aka.ms/AppRegInfo e tente novamente após a aprovação.";
        return L"Falha em " + std::wstring(step) + L" (HTTP " +
            std::to_wstring(response->status) + L").";
    }

    bool validUuid(std::string_view uuid)
    {
        return uuid.size() == 32 && std::all_of(uuid.begin(), uuid.end(), [](char c)
        { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                 (c >= 'A' && c <= 'F'); });
    }
}

std::wstring MicrosoftAuthService::clientId()
{
    DWORD size = GetEnvironmentVariableW(L"ETERNAL_LAUNCHER_MICROSOFT_CLIENT_ID", nullptr, 0);
    if (size < 2 || size > 128) return std::wstring(LauncherConfig::MicrosoftClientId);
    std::wstring value(size, L'\0');
    const DWORD actual = GetEnvironmentVariableW(L"ETERNAL_LAUNCHER_MICROSOFT_CLIENT_ID",
                                                 value.data(), size);
    if (!actual || actual >= size) return std::wstring(LauncherConfig::MicrosoftClientId);
    value.resize(actual);
    return value;
}

MicrosoftAuthResult MicrosoftAuthService::signIn(
    std::stop_token stopToken,
    const std::function<void(const MicrosoftDeviceCode&)>& onCode) const
{
    const auto clientIdValue = clientId();
    if (clientIdValue.empty()) return {false, {},
        L"Configure ETERNAL_LAUNCHER_MICROSOFT_CLIENT_ID com o ID do aplicativo Microsoft."};
    const auto id = urlEncode(narrow(clientIdValue));
    const auto base = L"https://login.microsoftonline.com/consumers/oauth2/v2.0/";
    const auto device = request(std::wstring(base) + L"devicecode", L"POST",
        L"application/x-www-form-urlencoded",
        "client_id=" + id + "&scope=XboxLive.signin%20offline_access");
    const auto deviceJson = parse(device);
    if (!device || device->status != 200 || !deviceJson)
        return {false, {}, failed(L"solicitar o código Microsoft", device)};
    const auto deviceCode = field(*deviceJson, "device_code");
    const auto userCode = field(*deviceJson, "user_code");
    const auto verificationUrl = field(*deviceJson, "verification_uri");
    if (deviceCode.empty() || userCode.empty() || verificationUrl.empty())
        return {false, {}, L"A Microsoft retornou um código de login incompleto."};
    onCode({wide(verificationUrl), wide(userCode)});
    const auto* seconds = deviceJson->find("expires_in");
    const auto* interval = deviceJson->find("interval");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(
        static_cast<int>(seconds ? seconds->numberValue(900) : 900));
    int pollSeconds = static_cast<int>(interval ? interval->numberValue(5) : 5);
    std::string microsoftToken;
    while (!stopToken.stop_requested() && std::chrono::steady_clock::now() < deadline)
    {
        for (int i = 0; i < pollSeconds && !stopToken.stop_requested(); ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        if (stopToken.stop_requested()) break;
        const auto token = request(std::wstring(base) + L"token", L"POST",
            L"application/x-www-form-urlencoded",
            "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code&client_id=" +
            id + "&device_code=" + urlEncode(deviceCode));
        const auto tokenJson = parse(token);
        if (!tokenJson) return {false, {}, failed(L"receber o token Microsoft", token)};
        if (token->status == 200) { microsoftToken = field(*tokenJson, "access_token"); break; }
        const auto error = field(*tokenJson, "error");
        if (error == "authorization_pending") continue;
        if (error == "slow_down") { pollSeconds += 5; continue; }
        if (error == "access_denied") return {false, {}, L"Login Microsoft cancelado."};
        return {false, {}, failed(L"entrar na conta Microsoft", token)};
    }
    if (microsoftToken.empty()) return {false, {}, L"O código Microsoft expirou ou o login foi cancelado."};

    const auto xbox = request(L"https://user.auth.xboxlive.com/user/authenticate", L"POST",
        L"application/json", "{\"Properties\":{\"AuthMethod\":\"RPS\",\"SiteName\":"
        "\"user.auth.xboxlive.com\",\"RpsTicket\":\"d=" + jsonEscape(microsoftToken) +
        "\"},\"RelyingParty\":\"http://auth.xboxlive.com\",\"TokenType\":\"JWT\"}");
    const auto xboxJson = parse(xbox);
    if (!xbox || xbox->status != 200 || !xboxJson) return {false, {}, failed(L"autenticar no Xbox Live", xbox)};
    const auto xboxToken = field(*xboxJson, "Token");
    if (xboxToken.empty()) return {false, {}, L"O Xbox Live não retornou um token válido."};
    const auto xsts = request(L"https://xsts.auth.xboxlive.com/xsts/authorize", L"POST",
        L"application/json", "{\"Properties\":{\"SandboxId\":\"RETAIL\",\"UserTokens\":[\"" +
        jsonEscape(xboxToken) + "\"]},\"RelyingParty\":\"rp://api.minecraftservices.com/\","
        "\"TokenType\":\"JWT\"}");
    const auto xstsJson = parse(xsts);
    if (!xsts || xsts->status != 200 || !xstsJson) return {false, {}, failed(L"autorizar o Xbox", xsts)};
    const auto xstsToken = field(*xstsJson, "Token");
    const auto* claims = xstsJson->find("DisplayClaims");
    const auto* xui = claims ? claims->find("xui") : nullptr;
    const auto* accounts = xui ? xui->arrayValue() : nullptr;
    const auto userHash = accounts && !accounts->empty() ? field(accounts->front(), "uhs") : "";
    if (xstsToken.empty() || userHash.empty())
        return {false, {}, L"O Xbox não retornou uma identidade válida."};
    const auto minecraft = request(L"https://api.minecraftservices.com/authentication/login_with_xbox",
        L"POST", L"application/json", "{\"identityToken\":\"XBL3.0 x=" +
        jsonEscape(userHash) + ";" + jsonEscape(xstsToken) + "\"}");
    const auto minecraftJson = parse(minecraft);
    if (!minecraft || minecraft->status != 200 || !minecraftJson)
        return {false, {}, failed(L"autenticar no Minecraft", minecraft)};
    const auto accessToken = field(*minecraftJson, "access_token");
    if (accessToken.empty()) return {false, {}, L"O Minecraft não retornou um token válido."};
    const auto profile = request(L"https://api.minecraftservices.com/minecraft/profile", L"GET",
        L"application/json", {}, wide(accessToken));
    const auto profileJson = parse(profile);
    if (!profile || profile->status != 200 || !profileJson)
        return {false, {}, L"A conta não possui um perfil Minecraft: Java Edition ativo."};
    const auto name = field(*profileJson, "name");
    const auto uuid = field(*profileJson, "id");
    if (name.empty() || !validUuid(uuid))
        return {false, {}, L"O perfil Minecraft retornado está incompleto."};
    return {true, {wide(name), wide(uuid), wide(accessToken), {}, clientIdValue},
            L"Conectado como " + wide(name) + L"."};
}
