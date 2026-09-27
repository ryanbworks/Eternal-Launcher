#pragma once

#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

class PlayPanelComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
              const LauncherViewState& state) const;
    [[nodiscard]] Gdiplus::RectF buttonBounds(const Gdiplus::RectF& bounds) const;
};
