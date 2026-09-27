#pragma once

#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

#include <string_view>
#include <memory>

namespace DrawingUtils
{
    std::unique_ptr<Gdiplus::GraphicsPath> createRoundedPath(
        const Gdiplus::RectF& bounds, float radius);
    void fillRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                              float radius, const Gdiplus::Color& color);
    void drawRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                              float radius, const Gdiplus::Color& color, float width = 1.0F);
    void drawText(Gdiplus::Graphics& graphics, std::wstring_view text,
                  const Gdiplus::RectF& bounds, float size, int style,
                  const Gdiplus::Color& color,
                  Gdiplus::StringAlignment horizontal = Gdiplus::StringAlignmentNear,
                  Gdiplus::StringAlignment vertical = Gdiplus::StringAlignmentNear);
    Gdiplus::Color colorForTone(StatusTone tone);
    bool contains(const Gdiplus::RectF& bounds, float x, float y);
}
