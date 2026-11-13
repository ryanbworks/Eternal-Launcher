#pragma once

#include "components/Buttons.h"
#include "services/SkinLookup.h"
#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct PlayerCardModel
{
    const LauncherViewState* state = nullptr;
    bool originalMode = false;
    std::wstring microsoftName;
    // Only drawn in previews; the live window shows the native EDIT instead.
    std::wstring previewNickname;
    bool nicknameValid = true;
    bool nicknameEmpty = false;
    float identitySelection = 0;
    // Face drawn in the avatar: the account's skin when the nickname is an original
    // account, otherwise the default face. `skinLoading` while the lookup runs.
    const SkinFace* face = nullptr;
    bool skinLoading = false;
    bool skinFound = false;
    // Replace the group-related status while the 1.8.9 is the selected game.
    std::wstring statusHeadline;
    std::wstring statusMessage;
    std::wstring playLabel;
    std::wstring lastPlayed;
    std::wstring totalPlayed;
    bool playing = false;
    // 0..100 fill of the main button while an operation reports progress, or negative.
    float progress = -1;
    float phase = 0;
    std::wstring updateWarning;
    bool installationVisible = false;
    // The file server is unreachable; offer to try again next to the status.
    bool serverOffline = false;
    // "This installation" summary: version, memory and mod counts of the selected game.
    std::wstring versionSummary;
    std::wstring memorySummary;
    std::wstring modsSummary;
    // Motion. `time` is a seconds clock for idle effects; `ambient` allows them (foreground,
    // animations on); `motion` allows any interpolated effect.
    float time = 0;
    bool ambient = false;
    bool motion = false;
    float playSweep = 1;   // 0..1 light band crossing the button after hover/focus
    float playPulse = 0;   // 1..0 ripple after a click
    float playLight = .5F; // pointer position across the button, 0..1
    float playPress = 0;   // 0..1 pressed compression
};

struct RankingRowModel
{
    int rank = 0;
    std::wstring name;
    std::wstring time;
    bool online = false;
    bool you = false;
    std::uint64_t seconds = 0;
};

// The top 3 wear gold, silver and bronze; their rows grow more ornate at each play-time
// milestone, so climbing hours is visible to everyone on Home.
namespace RankTier
{
    inline constexpr int Levels = 5;
    // Hours needed for levels 2..5 (level 1 is any time at all).
    inline constexpr std::uint64_t Hours[Levels - 1] = {25, 75, 150, 300};
    inline int level(std::uint64_t seconds)
    {
        int result = 1;
        for (const auto hours : Hours) if (seconds >= hours * 3600) ++result;
        return result;
    }
}

struct HomeModel
{
    PlayerCardModel player;
    std::vector<std::wstring> chips;
    float reveal = 1, energy = 0, phase = 0, drift = 0;
    // Newest unread item, shown over the landscape until it is read or dismissed.
    bool spotlight = false;
    NewsItem spotlightItem;
    int unreadNews = 0;
    int serverMods = -1;
    int userMods = -1;
    float entrance = 1;        // 0..1 staggered rise of tiles and news cards
    float spotlightReveal = 1; // 0..1 drop of the spotlight
    std::vector<RankingRowModel> ranking;
    std::wstring rankingNotice; // shown instead of the rows while there are none
    std::wstring rankingYou;
    float rankingScroll = 0;    // logical pixels scrolled down the player list
};

class HeroComponent;

class HomeView
{
public:
    void draw(Gdiplus::Graphics& graphics, const HomeModel& model, const HeroComponent& hero,
              const ControlVisuals& visuals, bool drawHero) const;
    static void drawPlayerCard(Gdiplus::Graphics& graphics, const PlayerCardModel& model, const ControlVisuals& visuals);
    static void drawAvatar(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, const Gdiplus::Color& status,
                           const SkinFace& face, float shimmer = -1);
    // The leaderboard lists every player; the wheel scrolls it inside its card.
    static constexpr float RankingRowHeight = 27;
    static Gdiplus::RectF rankingList();
    static float maximumRankingScroll(std::size_t rows);
private:
    static void drawPlayButton(Gdiplus::Graphics& graphics, const PlayerCardModel& model, const ControlVisuals& visuals);
    void drawTiles(Gdiplus::Graphics& graphics, const HomeModel& model) const;
    void drawRanking(Gdiplus::Graphics& graphics, const HomeModel& model) const;
    void drawSpotlight(Gdiplus::Graphics& graphics, const HomeModel& model, const ControlVisuals& visuals) const;
    static void drawPodiumRow(Gdiplus::Graphics& graphics, const Gdiplus::RectF& line, const RankingRowModel& row,
                              float time, bool ambient);
    void drawNews(Gdiplus::Graphics& graphics, const HomeModel& model, const HeroComponent& hero,
                  const ControlVisuals& visuals) const;
};
