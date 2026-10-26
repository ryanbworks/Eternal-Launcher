#include "services/PlaytimeStore.h"

#include "utils/JsonValue.h"

#include <fstream>
#include <sstream>
#include <system_error>

std::filesystem::path PlaytimeStore::filePath(const std::filesystem::path& directory)
{
    return directory / L"playtime.json";
}

PlaytimeRecord PlaytimeStore::load(const std::filesystem::path& directory)
{
    PlaytimeRecord record;
    std::ifstream input(filePath(directory), std::ios::binary);
    if (!input) return record;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const auto parsed = JsonValue::parse(buffer.str());
    if (!parsed || !parsed->objectValue()) return record;
    const auto number = [&](std::string_view key) {
        const auto* value = parsed->find(key);
        const double result = value ? value->numberValue(0) : 0;
        return result > 0 ? result : 0.0;
    };
    record.totalSeconds = static_cast<std::uint64_t>(number("totalSeconds"));
    record.lastSessionSeconds = static_cast<std::uint64_t>(number("lastSessionSeconds"));
    record.lastPlayed = static_cast<std::int64_t>(number("lastPlayed"));
    return record;
}

bool PlaytimeStore::save(const std::filesystem::path& directory, const PlaytimeRecord& record)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const auto target = filePath(directory);
    auto temporary = target;
    temporary += L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output << "{\"schemaVersion\":1,\"totalSeconds\":" << record.totalSeconds
               << ",\"lastSessionSeconds\":" << record.lastSessionSeconds
               << ",\"lastPlayed\":" << record.lastPlayed << "}\n";
        if (!output) return false;
    }
    std::filesystem::rename(temporary, target, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

std::wstring PlaytimeStore::formatDuration(std::uint64_t seconds)
{
    const auto minutes = seconds / 60;
    if (minutes < 1) return L"menos de 1 min";
    if (minutes < 60) return std::to_wstring(minutes) + L" min";
    const auto hours = minutes / 60;
    const auto rest = minutes % 60;
    if (hours >= 100 || rest == 0) return std::to_wstring(hours) + L" h";
    return std::to_wstring(hours) + L" h " + std::to_wstring(rest) + L" min";
}

std::wstring PlaytimeStore::formatLastPlayed(std::int64_t lastPlayed, std::int64_t now)
{
    if (lastPlayed <= 0) return L"Ainda não jogou";
    const auto elapsed = now > lastPlayed ? now - lastPlayed : 0;
    const auto minutes = elapsed / 60;
    if (minutes < 2) return L"Agora há pouco";
    if (minutes < 60) return L"Há " + std::to_wstring(minutes) + L" min";
    const auto hours = minutes / 60;
    if (hours < 24) return L"Há " + std::to_wstring(hours) + (hours == 1 ? L" hora" : L" horas");
    const auto days = hours / 24;
    if (days == 1) return L"Ontem";
    if (days < 30) return L"Há " + std::to_wstring(days) + L" dias";
    const auto months = days / 30;
    return L"Há " + std::to_wstring(months) + (months == 1 ? L" mês" : L" meses");
}

void PlaySession::start(PlaytimeRecord base, Clock::time_point now, std::int64_t wallNow)
{
    base_ = base;
    base_.lastSessionSeconds = 0;
    base_.lastPlayed = wallNow;
    started_ = now;
}

std::uint64_t PlaySession::elapsedSeconds(Clock::time_point now) const
{
    if (!started_ || now <= *started_) return 0;
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now - *started_).count());
}

PlaytimeRecord PlaySession::snapshot(Clock::time_point now, std::int64_t wallNow) const
{
    auto record = base_;
    const auto elapsed = elapsedSeconds(now);
    record.totalSeconds += elapsed;
    record.lastSessionSeconds = elapsed;
    if (started_) record.lastPlayed = wallNow;
    return record;
}
