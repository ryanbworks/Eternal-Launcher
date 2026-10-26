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
    // The file server is unreachable; offer to try again next to the status.
    bool serverOffline = false;
    // "This installation" summary: game version, memory and mod counts.
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

struct HomeModel
{
    PlayerCardModel player;
    std::vector<std::wstring> chips;
    float reveal = 1, energy = 0, phase = 0, drift = 0;
    bool toast = false;
    std::wstring toastText;
    int serverMods = -1;
    int userMods = -1;
    float entrance = 1;    // 0..1 staggered rise of tiles and news cards
    float toastReveal = 1; // 0..1 slide of the update toast
};

class HeroComponent;

class HomeView
{
public:
    void draw(Gdiplus::Graphics& graphics, const HomeModel& model, const HeroComponent& hero,
              const ControlVisuals& visuals, bool drawLeftColumn) const;
    static void drawPlayerCard(Gdiplus::Graphics& graphics, const PlayerCardModel& model, const ControlVisuals& visuals);
    static void drawAvatar(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, const Gdiplus::Color& status,
                           bool blink = false);
private:
    static void drawPlayButton(Gdiplus::Graphics& graphics, const PlayerCardModel& model, const ControlVisuals& visuals);
    void drawTiles(Gdiplus::Graphics& graphics, const HomeModel& model) const;
    void drawNews(Gdiplus::Graphics& graphics, const HomeModel& model, const HeroComponent& hero,
                  const ControlVisuals& visuals) const;
};
