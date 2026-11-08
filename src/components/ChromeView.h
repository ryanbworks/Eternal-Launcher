#pragma once

#include "components/Buttons.h"
#include "utils/BrandEmblem.h"

#include <objidl.h>
#include <gdiplus.h>

// The order follows the navigation rail; Help is anchored to its bottom.
enum class LauncherPage { Home, Versions, News, Mods, Settings, Help };

// Custom title bar and navigation rail, drawn in window-anchored logical units.
class ChromeView
{
public:
    // `indicatorY` is the top of the sliding selection behind the current page's icon.
    void draw(Gdiplus::Graphics& graphics, float width, float height, LauncherPage page, bool maximized,
              bool active, const ControlVisuals& visuals, float indicatorY) const;
private:
    BrandEmblem emblem_;
};
