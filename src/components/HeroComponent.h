#pragma once

#include <objidl.h>
#include <gdiplus.h>

class HeroComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds) const;
};
