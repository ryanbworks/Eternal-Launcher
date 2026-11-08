#pragma once

#include "services/PlaytimeStore.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

struct LeaderboardEntry
{
    int rank = 0;
    std::wstring name;
    std::uint64_t totalSeconds = 0;
    bool online = false;
};

struct LeaderboardResult
{
    bool success = false;
    std::vector<LeaderboardEntry> players;
    // The player the list was requested for; empty when no valid name was known.
    std::wstring playerName;
    bool hasYou = false;
    int yourRank = 0;
    std::uint64_t yourSeconds = 0;
};

struct PlaytimeApiResponse
{
    bool success = false;
    unsigned long statusCode = 0;
    std::string token;
    int heartbeatSeconds = 60;
    std::uint64_t totalSeconds = 0;
    bool importAvailable = false;
};

struct PlaytimeSyncUpdate
{
    std::wstring name;
    std::optional<std::uint64_t> totalSeconds;
    bool finished = false;
};

// Client of the server-side play time. The launcher only says "still playing";
// the server measures the time, so this code never sends a duration except for
// the one-time import of the counter kept by previous versions.
class PlaytimeApi
{
public:
    using UpdateCallback = std::function<void(std::optional<std::uint64_t> totalSeconds, bool finished)>;

    [[nodiscard]] static LeaderboardResult parseLeaderboard(std::string_view json);
    [[nodiscard]] static LeaderboardResult fetchLeaderboard(std::wstring_view baseUrl, std::wstring_view nickname,
        int limit, std::stop_token stop = {});

    [[nodiscard]] static PlaytimeApiResponse startSession(std::wstring_view baseUrl, std::wstring_view nickname, std::stop_token stop);
    [[nodiscard]] static PlaytimeApiResponse heartbeat(std::wstring_view baseUrl, const std::string& token, std::stop_token stop);
    [[nodiscard]] static PlaytimeApiResponse endSession(std::wstring_view baseUrl, const std::string& token);
    [[nodiscard]] static PlaytimeApiResponse importLocal(std::wstring_view baseUrl, const std::string& token,
        const PlaytimeRecord& local, std::stop_token stop);

    [[nodiscard]] static std::filesystem::path importMarkerPath(const std::filesystem::path& dataDirectory);

    // Runs on a worker for as long as the game is open: opens the session, performs the
    // import when it is still due, sends heartbeats and closes the session when stopped.
    static void runSession(std::wstring_view baseUrl, std::wstring_view nickname, const PlaytimeRecord& local,
        const std::filesystem::path& dataDirectory, const UpdateCallback& update, std::stop_token stop);
};
