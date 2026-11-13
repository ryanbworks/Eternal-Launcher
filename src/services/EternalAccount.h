#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

// A signed-in Conta Eternal (Drasl). The password is never kept: only the access
// token, which the launcher stores encrypted for the Windows user (DPAPI).
struct EternalSession
{
    std::wstring name;
    std::wstring uuid;         // 32 hex digits, no dashes
    std::string accessToken;
    std::string clientToken;
    [[nodiscard]] bool valid() const { return !name.empty() && uuid.size() == 32 && !accessToken.empty(); }
};

struct EternalAuthResult
{
    bool success = false;
    EternalSession session;
    std::wstring errorMessage;
    bool rejected = false;  // the server answered no (wrong password, expired token)
};

// The player's current skin as the account server publishes it.
struct EternalSkin
{
    bool custom = false;   // false: the player uses the default skin
    bool slim = false;
    std::string png;       // the texture file
};

namespace EternalAccount
{
    // https://accountseternal.aplot.cloud, or ETERNAL_ACCOUNTS_URL when set (local tests).
    std::wstring baseUrl();
    std::wstring registrationUrl(std::wstring_view baseUrl);
    std::wstring accountPageUrl(std::wstring_view baseUrl);
    // The javaagent argument value: "<jar>=<base>/authlib-injector".
    std::wstring agentArgument(const std::filesystem::path& jar, std::wstring_view baseUrl);
    std::string newClientToken();

    EternalAuthResult authenticate(std::wstring_view baseUrl, std::wstring_view username, std::wstring_view password,
                                   const std::string& clientToken, std::stop_token stop = {});
    // Confirms the token still works, refreshing it when the server allows.
    EternalAuthResult ensureValid(std::wstring_view baseUrl, const EternalSession& session, std::stop_token stop = {});
    void signOut(std::wstring_view baseUrl, const EternalSession& session);

    std::optional<EternalSkin> fetchSkin(std::wstring_view baseUrl, const std::wstring& uuid, std::stop_token stop = {});
    bool uploadSkin(std::wstring_view baseUrl, const EternalSession& session, const std::string& png, bool slim, std::wstring& error);
    bool resetSkin(std::wstring_view baseUrl, const EternalSession& session, std::wstring& error);

    // First access to an account the migration reserved: the player types only the
    // nickname and chooses a password (server/drasl/first_access.py on the file server).
    struct FirstAccess { bool known = false; bool pending = false; std::wstring name; std::wstring errorMessage; };
    FirstAccess checkFirstAccess(std::wstring_view fileServerUrl, std::wstring_view name, std::stop_token stop = {});
    bool claimFirstAccess(std::wstring_view fileServerUrl, std::wstring_view name, std::wstring_view password, std::wstring& error,
                          std::stop_token stop = {});
    // A new account with a player of the same name (open registration on Drasl).
    bool registerAccount(std::wstring_view baseUrl, std::wstring_view name, std::wstring_view password, std::wstring& error,
                         std::stop_token stop = {});
    // Drasl's English error messages in a player's words.
    std::wstring translateError(std::string_view message);

    // The pinned authlib-injector jar in the data folder, downloaded and checked
    // against its SHA-256 when missing. Empty with `error` set on failure.
    std::filesystem::path ensureAuthlibInjector(const std::filesystem::path& dataDirectory, std::wstring& error, std::stop_token stop = {});

    // Exposed for tests.
    EternalAuthResult parseAuthentication(std::string_view json, const std::string& clientToken);
    std::optional<EternalSkin> parseProfileTextures(std::string_view json, std::wstring& skinUrl);
    std::string multipartSkin(const std::string& boundary, const std::string& png, bool slim);
    // A Minecraft skin PNG: 64 x 64 or 64 x 32 (the server's default limit).
    bool validSkinPng(const std::string& png, std::wstring& error);
}

// The session on disk: name, UUID and client token in clear, the access token
// encrypted with DPAPI for the current Windows user (account.dat).
namespace EternalSessionStore
{
    std::optional<EternalSession> load(const std::filesystem::path& directory);
    bool save(const std::filesystem::path& directory, const EternalSession& session);
    void clear(const std::filesystem::path& directory);
}
