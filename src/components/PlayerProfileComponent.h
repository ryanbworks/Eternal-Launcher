#pragma once

#include <objidl.h>
#include <gdiplus.h>

class PlayerProfileComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds) const;
    [[nodiscard]] Gdiplus::RectF inputBounds(const Gdiplus::RectF& bounds) const;
};
