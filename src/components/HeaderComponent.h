#pragma once

#include <objidl.h>
#include <gdiplus.h>

class HeaderComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, float width) const;
    [[nodiscard]] Gdiplus::RectF settingsBounds(float width) const;
};
