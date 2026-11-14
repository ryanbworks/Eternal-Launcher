#pragma once

#include "components/Buttons.h"
#include "utils/BrandEmblem.h"

#include <objidl.h>
#include <gdiplus.h>

#include <iterator>

enum class LauncherPage { Home, Versions, News, Mods, Screenshots, Skins, Settings, Help };

// The navigation rail, top to bottom. Help is anchored to the rail's bottom instead.
inline constexpr LauncherPage NavigationOrder[] = {LauncherPage::Home, LauncherPage::Versions, LauncherPage::Skins,
    LauncherPage::Screenshots, LauncherPage::News, LauncherPage::Mods, LauncherPage::Settings};

// The rail slot of a page (Layout::navButton index), or -1 for Help.
inline int navigationSlot(LauncherPage page)
{
    for (int slot = 0; slot < static_cast<int>(std::size(NavigationOrder)); ++slot)
        if (NavigationOrder[slot] == page) return slot;
    return -1;
}

// Custom title bar and navigation rail, drawn in window-anchored logical units.
class ChromeView
{
public:
    // `indicatorY` is the top of the sliding selection behind the current page's icon.
    // `newsBadge` counts unread news on the News icon; `time` (seconds) pulses it.
    void draw(Gdiplus::Graphics& graphics, float width, float height, LauncherPage page, bool maximized,
              bool active, const ControlVisuals& visuals, float indicatorY, int newsBadge = 0, float time = -1) const;
private:
    BrandEmblem emblem_;
};
