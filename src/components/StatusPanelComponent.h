#pragma once

#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

#include <vector>

class StatusPanelComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
              const std::vector<StatusCardData>& cards) const;
};
