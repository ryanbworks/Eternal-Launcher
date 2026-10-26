#pragma once

#include "components/Buttons.h"
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
    std::wstring lastPlayed;
    std::wstring totalPlayed;
    bool playing = false;
    // 0..100 fill of the main button while an operation reports progress, or negative.
    float progress = -1;
    float phase = 0;
    std::wstring updateWarning;
    bool installationVisible = false;
    // "This installation" summary: game version, memory and mod counts.
    std::wstring versionSummary;
    std::wstring memorySummary;
    std::wstring modsSummary;
};

struct HomeModel
{
    PlayerCardModel player;
    std::vector<std::wstring> chips;
    float reveal = 1, energy = 0, phase = 0, drift = 0;
    bool toast = false;
    std::wstring toastText;
    int serverMods = -1;
    int userMods = -1;
};

class HeroComponent;

class HomeView
{
public:
    void draw(Gdiplus::Graphics& graphics, const HomeModel& model, const HeroComponent& hero,
              const ControlVisuals& visuals, bool drawLeftColumn) const;
    static void drawPlayerCard(Gdiplus::Graphics& graphics, const PlayerCardModel& model, const ControlVisuals& visuals);
    static void drawAvatar(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, const Gdiplus::Color& status);
private:
    void drawTiles(Gdiplus::Graphics& graphics, const LauncherViewState& state, int serverMods, int userMods) const;
    void drawNews(Gdiplus::Graphics& graphics, const LauncherViewState& state, const HeroComponent& hero,
                  const ControlVisuals& visuals) const;
};
