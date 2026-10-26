#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

struct PlaytimeRecord
{
    std::uint64_t totalSeconds = 0;
    std::uint64_t lastSessionSeconds = 0;
    // Seconds since the Unix epoch, UTC. Zero means the game was never played here.
    std::int64_t lastPlayed = 0;
};

// Persists total play time. A running session is flushed periodically so closing
// the launcher abruptly loses at most one interval.
class PlaytimeStore
{
public:
    static std::filesystem::path filePath(const std::filesystem::path& directory);
    static PlaytimeRecord load(const std::filesystem::path& directory);
    static bool save(const std::filesystem::path& directory, const PlaytimeRecord& record);
    static std::wstring formatDuration(std::uint64_t seconds);
    static std::wstring formatLastPlayed(std::int64_t lastPlayed, std::int64_t now);
};

class PlaySession
{
public:
    using Clock = std::chrono::steady_clock;
    void start(PlaytimeRecord base, Clock::time_point now, std::int64_t wallNow);
    // Returns the record including time played up to `now`.
    [[nodiscard]] PlaytimeRecord snapshot(Clock::time_point now, std::int64_t wallNow) const;
    [[nodiscard]] bool active() const { return started_.has_value(); }
    [[nodiscard]] std::uint64_t elapsedSeconds(Clock::time_point now) const;
    void stop() { started_.reset(); }
private:
    PlaytimeRecord base_;
    std::optional<Clock::time_point> started_;
};
