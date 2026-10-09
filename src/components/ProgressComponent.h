#pragma once

#include <objidl.h>
#include <gdiplus.h>

#include <string_view>

class ProgressComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
              int percent, std::wstring_view message, bool active, bool healthy,
              float displayedPercent = -1, float phase = 0,
              std::wstring_view title = {}, bool recoveryLink = false) const;
};
