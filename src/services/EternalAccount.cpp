#include "services/EternalAccount.h"

#include "config/LauncherConfig.h"
#include "services/HttpDownloadClient.h"
#include "utils/FileHash.h"
#include "utils/JsonValue.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>

namespace
{
    std::string utf8(std::wstring_view value)
    {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(std::max(size, 0)), '\0');
        if (size > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring wide(std::string_view value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring result(static_cast<std::size_t>(std::max(size, 0)), L'\0');
        if (size > 0) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
        return result;
    }

    // A JSON string literal, quotes included.
    std::string json(std::string_view value)
    {
        std::string out = "\"";
        for (const char c : value) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char escaped[8];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += escaped;
                } else {
                    out.push_back(c);
                }
            }
        }
        return out + "\"";
    }

    std::wstring trimmed(std::wstring url)
    {
        while (!url.empty() && url.back() == L'/') url.pop_back();
        return url;
    }

    bool hex32(std::wstring_view value)
    {
        return value.size() == 32 && std::all_of(value.begin(), value.end(), [](wchar_t c) {
            return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
        });
    }

    std::string base64Decode(const std::string& text)
    {
        DWORD size = 0;
        if (!CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr)) return {};
        std::string bytes(size, '\0');
        if (!CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64,
                reinterpret_cast<BYTE*>(bytes.data()), &size, nullptr, nullptr)) return {};
        bytes.resize(size);
        return bytes;
    }

    std::string base64Encode(const std::string& bytes)
    {
        DWORD size = 0;
        const auto flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
        if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()), flags, nullptr, &size)) return {};
        std::string text(size, '\0');
        if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()), flags, text.data(), &size)) return {};
        text.resize(size);
        return text;
    }

    std::wstring bearer(const EternalSession& session) { return L"Authorization: Bearer " + wide(session.accessToken) + L"\r\n"; }

    // The Yggdrasil error message ("Invalid credentials...") in a player's words.
    std::wstring authError(const HttpDownloadResult& response)
    {
        if (response.statusCode == 0) return L"Sem conexão com o servidor de contas. Confira sua internet.";
        if (response.statusCode == 403 || response.statusCode == 401) return L"Usuário ou senha incorretos.";
        if (response.statusCode == 429) return L"Muitas tentativas. Espere um pouco e tente de novo.";
        return L"O servidor de contas respondeu com erro " + std::to_wstring(response.statusCode) + L". Tente mais tarde.";
    }
}

std::wstring EternalAccount::baseUrl()
{
    std::array<wchar_t, 512> value{};
    const DWORD length = GetEnvironmentVariableW(L"ETERNAL_ACCOUNTS_URL", value.data(), static_cast<DWORD>(value.size()));
    if (length > 0 && length < value.size()) return trimmed(std::wstring(value.data(), length));
    return std::wstring(LauncherConfig::AccountsBaseUrl);
}

std::wstring EternalAccount::registrationUrl(std::wstring_view baseUrl) { return trimmed(std::wstring(baseUrl)) + L"/web/registration"; }
std::wstring EternalAccount::accountPageUrl(std::wstring_view baseUrl) { return trimmed(std::wstring(baseUrl)) + L"/web/user"; }

std::wstring EternalAccount::agentArgument(const std::filesystem::path& jar, std::wstring_view baseUrl)
{
    return jar.wstring() + L"=" + trimmed(std::wstring(baseUrl)) + L"/authlib-injector";
}

std::string EternalAccount::newClientToken()
{
    std::array<unsigned char, 16> bytes{};
    BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    std::string token;
    for (const auto byte : bytes) {
        char pair[3];
        std::snprintf(pair, sizeof(pair), "%02x", byte);
        token += pair;
    }
    return token;
}

EternalAuthResult EternalAccount::parseAuthentication(std::string_view text, const std::string& clientToken)
{
    EternalAuthResult result;
    const auto document = JsonValue::parse(text);
    const auto* access = document ? document->find("accessToken") : nullptr;
    const auto* profile = document ? document->find("selectedProfile") : nullptr;
    const auto* id = profile ? profile->find("id") : nullptr;
    const auto* name = profile ? profile->find("name") : nullptr;
    if (!access || !access->stringValue() || !id || !id->stringValue() || !name || !name->stringValue()) {
        result.errorMessage = L"Esta conta ainda não tem um jogador. Crie um no site de contas.";
        return result;
    }
    result.session.accessToken = *access->stringValue();
    result.session.uuid = wide(*id->stringValue());
    result.session.uuid.erase(std::remove(result.session.uuid.begin(), result.session.uuid.end(), L'-'), result.session.uuid.end());
    std::transform(result.session.uuid.begin(), result.session.uuid.end(), result.session.uuid.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    result.session.name = wide(*name->stringValue());
    const auto* client = document->find("clientToken");
    result.session.clientToken = client && client->stringValue() ? *client->stringValue() : clientToken;
    if (!hex32(result.session.uuid) || result.session.name.empty()) {
        result.errorMessage = L"O servidor de contas respondeu de um jeito inesperado.";
        return result;
    }
    result.success = true;
    return result;
}

EternalAuthResult EternalAccount::authenticate(std::wstring_view baseUrl, std::wstring_view username, std::wstring_view password,
                                               const std::string& clientToken, std::stop_token stop)
{
    const std::string body = "{\"agent\":{\"name\":\"Minecraft\",\"version\":1},\"username\":" + json(utf8(username)) +
        ",\"password\":" + json(utf8(password)) + ",\"clientToken\":" + json(clientToken) + ",\"requestUser\":true}";
    const auto response = HttpDownloadClient{}.send(L"POST", trimmed(std::wstring(baseUrl)) + L"/authlib-injector/authserver/authenticate",
        {}, &body, L"application/json", stop);
    if (!response.success) {
        EternalAuthResult result;
        result.rejected = response.statusCode == 403 || response.statusCode == 401;
        result.errorMessage = authError(response);
        return result;
    }
    return parseAuthentication(response.content, clientToken);
}

EternalAuthResult EternalAccount::ensureValid(std::wstring_view baseUrl, const EternalSession& session, std::stop_token stop)
{
    const auto base = trimmed(std::wstring(baseUrl));
    const std::string tokens = "{\"accessToken\":" + json(session.accessToken) + ",\"clientToken\":" + json(session.clientToken) + "}";
    HttpDownloadClient client;
    const auto validation = client.send(L"POST", base + L"/authlib-injector/authserver/validate", {}, &tokens, L"application/json", stop);
    EternalAuthResult result;
    if (validation.success) { result.success = true; result.session = session; return result; }
    if (validation.statusCode == 0) { result.errorMessage = authError(validation); return result; }
    // Expired: one refresh keeps the player signed in without the password.
    const std::string refresh = "{\"accessToken\":" + json(session.accessToken) + ",\"clientToken\":" + json(session.clientToken) +
        ",\"requestUser\":true}";
    const auto refreshed = client.send(L"POST", base + L"/authlib-injector/authserver/refresh", {}, &refresh, L"application/json", stop);
    if (!refreshed.success) {
        result.rejected = refreshed.statusCode == 403 || refreshed.statusCode == 401;
        result.errorMessage = result.rejected ? L"Sua sessão expirou. Entre de novo." : authError(refreshed);
        return result;
    }
    return parseAuthentication(refreshed.content, session.clientToken);
}

void EternalAccount::signOut(std::wstring_view baseUrl, const EternalSession& session)
{
    const std::string tokens = "{\"accessToken\":" + json(session.accessToken) + ",\"clientToken\":" + json(session.clientToken) + "}";
    (void)HttpDownloadClient{}.send(L"POST", trimmed(std::wstring(baseUrl)) + L"/authlib-injector/authserver/invalidate", {}, &tokens,
        L"application/json");
}

std::optional<EternalSkin> EternalAccount::parseProfileTextures(std::string_view text, std::wstring& skinUrl)
{
    skinUrl.clear();
    const auto document = JsonValue::parse(text);
    const auto* properties = document ? document->find("properties") : nullptr;
    const auto* list = properties ? properties->arrayValue() : nullptr;
    if (!list) return std::nullopt;
    for (const auto& property : *list) {
        const auto* name = property.find("name");
        const auto* value = property.find("value");
        if (!name || !name->stringValue() || *name->stringValue() != "textures" || !value || !value->stringValue()) continue;
        const auto textures = JsonValue::parse(base64Decode(*value->stringValue()));
        const auto* all = textures ? textures->find("textures") : nullptr;
        const auto* skin = all ? all->find("SKIN") : nullptr;
        EternalSkin result;
        if (!skin) return result;  // no SKIN entry: the default skin
        const auto* url = skin->find("url");
        if (!url || !url->stringValue()) return result;
        skinUrl = wide(*url->stringValue());
        const auto* metadata = skin->find("metadata");
        const auto* model = metadata ? metadata->find("model") : nullptr;
        result.slim = model && model->stringValue() && *model->stringValue() == "slim";
        result.custom = true;
        return result;
    }
    return EternalSkin{};
}

std::optional<EternalSkin> EternalAccount::fetchSkin(std::wstring_view baseUrl, const std::wstring& uuid, std::stop_token stop)
{
    if (!hex32(uuid)) return std::nullopt;
    const auto base = trimmed(std::wstring(baseUrl));
    HttpDownloadClient client;
    const auto profile = client.getText(base + L"/authlib-injector/sessionserver/session/minecraft/profile/" + uuid, stop);
    if (!profile.success) return std::nullopt;
    std::wstring url;
    auto skin = parseProfileTextures(profile.content, url);
    if (!skin || !skin->custom) return skin;
    // Textures are only downloaded from the account server itself.
    if (url.rfind(base + L"/", 0) != 0) return EternalSkin{};
    const auto texture = client.getText(url, stop);
    if (!texture.success) return std::nullopt;
    skin->png = texture.content;
    return skin;
}

std::vector<EternalAccount::Profile> EternalAccount::parseProfiles(std::string_view text)
{
    std::vector<Profile> result;
    const auto document = JsonValue::parse(text);
    const auto* list = document ? document->arrayValue() : nullptr;
    if (!list) return result;
    for (const auto& entry : *list) {
        const auto* id = entry.find("id");
        const auto* name = entry.find("name");
        if (!id || !id->stringValue() || !name || !name->stringValue()) continue;
        Profile profile{wide(*name->stringValue()), wide(*id->stringValue())};
        if (!hex32(profile.uuid) || profile.name.empty() || profile.name.size() > 16) continue;
        std::transform(profile.uuid.begin(), profile.uuid.end(), profile.uuid.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        result.push_back(std::move(profile));
    }
    return result;
}

std::vector<EternalAccount::Profile> EternalAccount::lookupProfiles(std::wstring_view baseUrl, const std::vector<std::wstring>& names,
                                                                    std::stop_token stop)
{
    std::vector<Profile> result;
    // The batch lookup takes a handful of names per request, like Mojang's.
    for (std::size_t start = 0; start < names.size() && !stop.stop_requested(); start += 10) {
        std::string body = "[";
        for (std::size_t i = start; i < std::min(names.size(), start + 10); ++i) body += (i > start ? "," : "") + json(utf8(names[i]));
        body += "]";
        const auto response = HttpDownloadClient{}.send(L"POST", trimmed(std::wstring(baseUrl)) + L"/authlib-injector/api/profiles/minecraft", {},
            &body, L"application/json", stop);
        if (!response.success) continue;
        for (auto& profile : parseProfiles(response.content)) result.push_back(std::move(profile));
    }
    return result;
}

std::string EternalAccount::multipartSkin(const std::string& boundary, const std::string& png, bool slim)
{
    std::string body;
    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"variant\"\r\n\r\n";
    body += slim ? "slim" : "classic";
    body += "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"skin.png\"\r\nContent-Type: image/png\r\n\r\n";
    body += png;
    body += "\r\n--" + boundary + "--\r\n";
    return body;
}

bool EternalAccount::validSkinPng(const std::string& png, std::wstring& error)
{
    // PNG signature, then the IHDR chunk with width and height (big-endian).
    static const unsigned char Signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (png.size() < 24 || !std::equal(std::begin(Signature), std::end(Signature), reinterpret_cast<const unsigned char*>(png.data())) ||
        png.compare(12, 4, "IHDR") != 0) {
        error = L"A skin precisa ser um arquivo PNG.";
        return false;
    }
    const auto byte = [&png](std::size_t i) { return static_cast<unsigned>(static_cast<unsigned char>(png[i])); };
    const unsigned width = byte(16) << 24 | byte(17) << 16 | byte(18) << 8 | byte(19);
    const unsigned height = byte(20) << 24 | byte(21) << 16 | byte(22) << 8 | byte(23);
    if (width != 64 || (height != 64 && height != 32)) {
        error = L"A skin precisa ter 64 × 64 pixels (ou 64 × 32, formato antigo). Esta tem " +
            std::to_wstring(width) + L" × " + std::to_wstring(height) + L".";
        return false;
    }
    if (png.size() > 512 * 1024) { error = L"Este arquivo é grande demais para uma skin."; return false; }
    return true;
}

bool EternalAccount::uploadSkin(std::wstring_view baseUrl, const EternalSession& session, const std::string& png, bool slim, std::wstring& error)
{
    if (!validSkinPng(png, error)) return false;
    const std::string boundary = "EternalSkin" + newClientToken();
    const auto body = multipartSkin(boundary, png, slim);
    const auto response = HttpDownloadClient{}.send(L"POST", trimmed(std::wstring(baseUrl)) + L"/services/minecraft/profile/skins",
        bearer(session), &body, wide("multipart/form-data; boundary=" + boundary));
    if (response.success) return true;
    error = response.statusCode == 401 || response.statusCode == 403 ? L"Sua sessão expirou. Entre de novo para trocar a skin."
        : response.statusCode == 400 ? L"O servidor recusou esta skin. Confira se é uma skin de Minecraft."
        : L"Não foi possível trocar a skin agora. Tente de novo.";
    return false;
}

bool EternalAccount::resetSkin(std::wstring_view baseUrl, const EternalSession& session, std::wstring& error)
{
    const auto response = HttpDownloadClient{}.send(L"DELETE", trimmed(std::wstring(baseUrl)) + L"/services/minecraft/profile/skins/active",
        bearer(session));
    if (response.success) return true;
    error = response.statusCode == 401 || response.statusCode == 403 ? L"Sua sessão expirou. Entre de novo."
        : L"Não foi possível voltar para a skin padrão agora.";
    return false;
}

std::wstring EternalAccount::translateError(std::string_view message)
{
    const auto has = [&message](std::string_view part) { return message.find(part) != std::string_view::npos; };
    if (has("username is taken") || has("player name is taken")) return L"Esse nick já tem conta. Se é seu, use Entrar.";
    if (has("Invalid password") && (has("longer") || has("short"))) return L"A senha precisa ter pelo menos 8 caracteres.";
    if (has("Invalid password")) return L"Essa senha não pode ser usada. Tente outra.";
    if (has("Invalid username") || has("Invalid player name")) return L"Use de 3 a 16 letras, números ou _ no nick.";
    if (has("Invalid credentials")) return L"Usuário ou senha incorretos.";
    if (has("registration") || has("not allowed")) return L"O cadastro está fechado no momento.";
    return L"O servidor de contas recusou o pedido. Tente de novo.";
}

EternalAccount::FirstAccess EternalAccount::checkFirstAccess(std::wstring_view fileServerUrl, std::wstring_view name, std::stop_token stop)
{
    FirstAccess result;
    const auto response = HttpDownloadClient{}.send(L"GET", trimmed(std::wstring(fileServerUrl)) + L"/api/accounts/first-access/" + std::wstring(name),
        {}, nullptr, {}, stop);
    if (!response.success) {
        result.errorMessage = response.statusCode == 0 ? L"Sem conexão com o servidor. Confira sua internet." : L"Não foi possível conferir o nick agora.";
        return result;
    }
    const auto document = JsonValue::parse(response.content);
    const auto* pending = document ? document->find("firstAccess") : nullptr;
    const auto* exact = document ? document->find("name") : nullptr;
    result.known = pending != nullptr;
    result.pending = pending && pending->boolValue(false);
    if (exact && exact->stringValue()) result.name = wide(*exact->stringValue());
    return result;
}

bool EternalAccount::claimFirstAccess(std::wstring_view fileServerUrl, std::wstring_view name, std::wstring_view password, std::wstring& error,
                                      std::stop_token stop)
{
    const std::string body = "{\"name\":" + json(utf8(name)) + ",\"password\":" + json(utf8(password)) + "}";
    const auto response = HttpDownloadClient{}.send(L"POST", trimmed(std::wstring(fileServerUrl)) + L"/api/accounts/first-access", {}, &body,
        L"application/json", stop);
    if (response.success) return true;
    error = response.statusCode == 409 ? L"Esse nick já criou a senha. Use Entrar com a sua senha."
        : response.statusCode == 400 ? L"Escolha uma senha nova com pelo menos 8 caracteres."
        : response.statusCode == 429 ? L"Muitas tentativas. Espere um minuto e tente de novo."
        : response.statusCode == 0 ? L"Sem conexão com o servidor. Confira sua internet."
        : L"Não foi possível criar a senha agora. Tente de novo.";
    return false;
}

bool EternalAccount::registerAccount(std::wstring_view baseUrl, std::wstring_view name, std::wstring_view password, std::wstring& error,
                                     std::stop_token stop)
{
    const std::string body = "{\"username\":" + json(utf8(name)) + ",\"password\":" + json(utf8(password)) + ",\"playerName\":" + json(utf8(name)) + "}";
    const auto response = HttpDownloadClient{}.send(L"POST", trimmed(std::wstring(baseUrl)) + L"/drasl/api/v3/users", {}, &body,
        L"application/json", stop);
    if (response.success) return true;
    if (response.statusCode == 0) { error = L"Sem conexão com o servidor de contas. Confira sua internet."; return false; }
    if (response.statusCode == 429) { error = L"Muitas tentativas. Espere um minuto e tente de novo."; return false; }
    const auto document = JsonValue::parse(response.content);
    const auto* message = document ? document->find("message") : nullptr;
    error = translateError(message && message->stringValue() ? *message->stringValue() : std::string{});
    return false;
}

std::filesystem::path EternalAccount::ensureAuthlibInjector(const std::filesystem::path& dataDirectory, std::wstring& error, std::stop_token stop)
{
    const auto directory = dataDirectory / L"runtime" / L"authlib-injector";
    const auto jar = directory / std::wstring(LauncherConfig::AuthlibInjectorFileName);
    const std::string expected(LauncherConfig::AuthlibInjectorSha256);
    std::error_code ignored;
    if (std::filesystem::is_regular_file(jar, ignored) && FileHash::sha256(jar, stop) == expected) return jar;
    std::filesystem::create_directories(directory, ignored);
    const auto partial = directory / (std::wstring(LauncherConfig::AuthlibInjectorFileName) + L".part");
    for (const auto url : {LauncherConfig::AuthlibInjectorUrl, LauncherConfig::AuthlibInjectorMirrorUrl}) {
        if (stop.stop_requested()) break;
        const auto download = HttpDownloadClient{}.downloadFile(url, partial, stop, {});
        // Nothing is used before its hash matches the version compiled into the launcher.
        if (download.success && FileHash::sha256(partial, stop) == expected) {
            std::filesystem::rename(partial, jar, ignored);
            if (!ignored) return jar;
        }
        std::filesystem::remove(partial, ignored);
    }
    error = L"Não foi possível baixar o componente de login da Conta Eternal. Confira sua internet.";
    return {};
}

// ---------------------------------------------------------------------------------------------

std::optional<EternalSession> EternalSessionStore::load(const std::filesystem::path& directory)
{
    std::ifstream input(directory / L"account.dat", std::ios::binary);
    std::string name, uuid, clientToken, sealed;
    if (!std::getline(input, name) || !std::getline(input, uuid) || !std::getline(input, clientToken) || !std::getline(input, sealed))
        return std::nullopt;
    for (auto* line : {&name, &uuid, &clientToken, &sealed}) if (!line->empty() && line->back() == '\r') line->pop_back();
    const auto encrypted = base64Decode(sealed);
    DATA_BLOB in{static_cast<DWORD>(encrypted.size()), reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()))};
    DATA_BLOB out{};
    if (encrypted.empty() || !CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return std::nullopt;
    EternalSession session;
    session.accessToken.assign(reinterpret_cast<const char*>(out.pbData), out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    session.name = wide(name);
    session.uuid = wide(uuid);
    session.clientToken = clientToken;
    if (!session.valid()) return std::nullopt;
    return session;
}

bool EternalSessionStore::save(const std::filesystem::path& directory, const EternalSession& session)
{
    DATA_BLOB in{static_cast<DWORD>(session.accessToken.size()), reinterpret_cast<BYTE*>(const_cast<char*>(session.accessToken.data()))};
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"Eternal Launcher", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    const auto sealed = base64Encode(std::string(reinterpret_cast<const char*>(out.pbData), out.cbData));
    LocalFree(out.pbData);
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    const auto file = directory / L"account.dat";
    const auto partial = directory / L"account.dat.part";
    {
        std::ofstream output(partial, std::ios::binary | std::ios::trunc);
        output << utf8(session.name) << '\n' << utf8(session.uuid) << '\n' << session.clientToken << '\n' << sealed << '\n';
        if (!output) return false;
    }
    std::filesystem::rename(partial, file, ignored);
    return !ignored;
}

void EternalSessionStore::clear(const std::filesystem::path& directory)
{
    std::error_code ignored;
    std::filesystem::remove(directory / L"account.dat", ignored);
}
