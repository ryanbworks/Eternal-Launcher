#pragma once

#include "utils/Icons.h"

#include <objidl.h>
#include <gdiplus.h>

#include <functional>
#include <optional>
#include <string_view>

// Interaction state of one native owner-drawn control, read by the views while drawing.
struct ControlVisual
{
    float hover = 0;
    bool focused = false;
    bool pressed = false;
    bool enabled = true;
};
using ControlVisuals = std::function<ControlVisual(int id)>;

namespace Buttons
{
    enum class Style { Primary, Secondary, Ghost, Danger, Link, Tab, Chip };

    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, std::wstring_view label,
              const ControlVisual& visual, Style style = Style::Secondary,
              std::optional<Icons::Icon> icon = std::nullopt, bool selected = false, float labelSize = 13.0F);
    // Icon-only square button (title bar, sidebar, small inline actions).
    void drawIcon(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, Icons::Icon icon,
                  const ControlVisual& visual, bool selected = false, float iconSize = 20.0F,
                  const Gdiplus::Color* hoverFill = nullptr);
    void drawSwitch(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, bool on,
                    const ControlVisual& visual);
}
