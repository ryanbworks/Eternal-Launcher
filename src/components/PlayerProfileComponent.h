#pragma once

#include <objidl.h>
#include <gdiplus.h>
#include <string_view>

class PlayerProfileComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
              bool original, std::wstring_view playerName) const;
    [[nodiscard]] Gdiplus::RectF inputBounds(const Gdiplus::RectF& bounds) const;
    [[nodiscard]] Gdiplus::RectF offlineTabBounds(const Gdiplus::RectF& bounds) const;
    [[nodiscard]] Gdiplus::RectF originalTabBounds(const Gdiplus::RectF& bounds) const;
};
