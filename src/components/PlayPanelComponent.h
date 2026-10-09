#pragma once

#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

class PlayPanelComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
              const LauncherViewState& state, float hover = 0, bool focused = false,
              bool pressed = false,float compression = 0,float sweep = 1,float pulse = 0,
              float light = .5F,float phase = 0,bool effects = false) const;
    [[nodiscard]] Gdiplus::RectF buttonBounds(const Gdiplus::RectF& bounds) const;
};
