#include "services/PlaytimeApi.h"

#include "services/HttpDownloadClient.h"
#include "utils/JsonValue.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <system_error>

namespace
{
    // Nicknames only ever contain ASCII letters, digits and underscores.
    std::string asciiNickname(std::wstring_view nickname)
    {
        std::string result;
        result.reserve(nickname.size());
        for (const wchar_t c : nickname) {
            const bool allowed = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_';
            if (!allowed) return {};
            result += static_cast<char>(c);
        }
        return result.size() >= 3 && result.size() <= 16 ? result : std::string{};
    }

    std::wstring asciiWide(const std::string& value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (const char c : value) result += static_cast<wchar_t>(static_cast<unsigned char>(c));
        return result;
    }

    std::uint64_t seconds(const JsonValue& object, std::string_view key)
    {
        const auto* value = object.find(key);
        const double number = value ? value->numberValue(0) : 0;
        return number > 0 ? static_cast<std::uint64_t>(number) : 0;
    }

    std::wstring endpoint(std::wstring_view baseUrl, std::wstring_view path)
    {
        std::wstring url(baseUrl);
        if (!url.empty() && url.back() == L'/') url.pop_back();
        return url + L"/api/playtime/" + std::wstring(path);
    }

    PlaytimeApiResponse toResponse(const HttpDownloadResult& http)
    {
        PlaytimeApiResponse response;
        response.statusCode = http.statusCode;
        if (!http.success) return response;
        const auto document = JsonValue::parse(http.content);
        if (!document || !document->objectValue()) return response;
        response.success = true;
        response.totalSeconds = seconds(*document, "totalSeconds");
        if (const auto* token = document->find("token"); token && token->stringValue()) response.token = *token->stringValue();
        if (const auto* interval = document->find("heartbeatSeconds"))
            response.heartbeatSeconds = std::clamp(static_cast<int>(interval->numberValue(60)), 15, 300);
        if (const auto* available = document->find("importAvailable")) response.importAvailable = available->boolValue(false);
        return response;
    }

    std::string tokenBody(const std::string& token) { return "{\"token\":\"" + token + "\"}"; }
}

LeaderboardResult PlaytimeApi::parseLeaderboard(std::string_view json)
{
    LeaderboardResult result;
    const auto document = JsonValue::parse(json);
    if (!document || !document->objectValue()) return result;
    const auto* players = document->find("players");
    const auto* list = players ? players->arrayValue() : nullptr;
    if (!list) return result;
    for (const auto& entry : *list) {
        const auto* name = entry.objectValue() ? entry.find("name") : nullptr;
        if (!name || !name->stringValue()) continue;
        // Anything but a valid nickname is not drawn, whatever the server sent.
        if (asciiNickname(asciiWide(*name->stringValue())).empty()) continue;
        LeaderboardEntry item;
        item.name = asciiWide(*name->stringValue());
        item.totalSeconds = seconds(entry, "totalSeconds");
        item.rank = static_cast<int>(seconds(entry, "rank"));
        if (const auto* online = entry.find("online")) item.online = online->boolValue(false);
        result.players.push_back(std::move(item));
        if (result.players.size() == 20) break;
    }
    if (const auto* you = document->find("you"); you && you->objectValue()) {
        result.hasYou = true;
        result.yourRank = static_cast<int>(seconds(*you, "rank"));
        result.yourSeconds = seconds(*you, "totalSeconds");
    }
    result.success = true;
    return result;
}

LeaderboardResult PlaytimeApi::fetchLeaderboard(std::wstring_view baseUrl, std::wstring_view nickname, int limit, std::stop_token stop)
{
    auto url = endpoint(baseUrl, L"leaderboard?limit=") + std::to_wstring(limit);
    const auto name = asciiNickname(nickname);
    if (!name.empty()) url += L"&player=" + asciiWide(name);
    const auto response = HttpDownloadClient{}.getText(url, stop);
    if (!response.success) return {};
    auto result = parseLeaderboard(response.content);
    if (!name.empty()) result.playerName = nickname;
    return result;
}

PlaytimeApiResponse PlaytimeApi::startSession(std::wstring_view baseUrl, std::wstring_view nickname, std::stop_token stop)
{
    const auto name = asciiNickname(nickname);
    if (name.empty()) return {};
    return toResponse(HttpDownloadClient{}.postJson(endpoint(baseUrl, L"session/start"), "{\"nickname\":\"" + name + "\"}", stop));
}

PlaytimeApiResponse PlaytimeApi::heartbeat(std::wstring_view baseUrl, const std::string& token, std::stop_token stop)
{
    return toResponse(HttpDownloadClient{}.postJson(endpoint(baseUrl, L"session/heartbeat"), tokenBody(token), stop));
}

PlaytimeApiResponse PlaytimeApi::endSession(std::wstring_view baseUrl, const std::string& token)
{
    // Best effort: if this fails the server only loses the time since the last heartbeat,
    // so closing the launcher must never wait long for it.
    return toResponse(HttpDownloadClient{}.postJson(endpoint(baseUrl, L"session/end"), tokenBody(token), {}, 2000));
}

PlaytimeApiResponse PlaytimeApi::importLocal(std::wstring_view baseUrl, const std::string& token, const PlaytimeRecord& local, std::stop_token stop)
{
    const auto body = "{\"token\":\"" + token + "\",\"totalSeconds\":" + std::to_string(local.totalSeconds) +
        ",\"lastPlayed\":" + std::to_string(local.lastPlayed) + "}";
    return toResponse(HttpDownloadClient{}.postJson(endpoint(baseUrl, L"import"), body, stop));
}

std::filesystem::path PlaytimeApi::importMarkerPath(const std::filesystem::path& dataDirectory)
{
    return dataDirectory / L"playtime-imported.marker";
}

void PlaytimeApi::runSession(std::wstring_view baseUrl, std::wstring_view nickname, const PlaytimeRecord& local,
    const std::filesystem::path& dataDirectory, const UpdateCallback& update, std::stop_token stop)
{
    std::mutex mutex;
    std::condition_variable_any wake;
    const auto pause = [&](int duration) {
        std::unique_lock lock(mutex);
        (void)wake.wait_for(lock, stop, std::chrono::seconds(duration), [] { return false; });
    };

    std::string token;
    int interval = 60;
    while (!stop.stop_requested()) {
        if (token.empty()) {
            const auto started = startSession(baseUrl, nickname, stop);
            if (!started.success || started.token.empty()) { pause(60); continue; }
            token = started.token;
            interval = started.heartbeatSeconds;
            update(started.totalSeconds, false);
            std::error_code error;
            const auto marker = importMarkerPath(dataDirectory);
            if (started.importAvailable && local.totalSeconds > 0 && !std::filesystem::exists(marker, error)) {
                const auto imported = importLocal(baseUrl, token, local, stop);
                if (imported.success) {
                    std::ofstream(marker, std::ios::binary | std::ios::trunc) << "imported\n";
                    update(imported.totalSeconds, false);
                }
            }
        } else {
            const auto beat = heartbeat(baseUrl, token, stop);
            if (beat.statusCode == 404) { token.clear(); continue; } // Expired or replaced: open another one.
            if (beat.success) update(beat.totalSeconds, false);
        }
        pause(interval);
    }
    std::optional<std::uint64_t> total;
    if (!token.empty()) {
        const auto ended = endSession(baseUrl, token);
        if (ended.success) total = ended.totalSeconds;
    }
    update(total, true);
}
