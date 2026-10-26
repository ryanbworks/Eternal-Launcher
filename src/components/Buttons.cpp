#include "components/Buttons.h"

#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <algorithm>

using DrawingUtils::blend;

void Buttons::draw(Gdiplus::Graphics& g, const Gdiplus::RectF& b, std::wstring_view label,
                   const ControlVisual& v, Style style, std::optional<Icons::Icon> icon, bool selected, float labelSize)
{
    const float hover = v.enabled ? std::clamp(v.hover, 0.0F, 1.0F) : 0.0F;
    Gdiplus::Color fill(0, 0, 0, 0), outline(0, 0, 0, 0), ink = Theme::TextPrimary;
    float radius = Theme::ControlRadius;
    switch (style)
    {
    case Style::Primary:
        fill = v.enabled ? blend(Theme::Accent, Theme::AccentLight, v.pressed ? 1.0F : hover) : Theme::SurfaceHover;
        ink = v.enabled ? Theme::AccentInk : Theme::TextMuted;
        break;
    case Style::Secondary:
        fill = blend(Theme::SurfaceElevated, Theme::SurfaceHover, v.pressed ? 1.0F : hover);
        ink = v.enabled ? Theme::TextPrimary : Theme::TextMuted;
        break;
    case Style::Ghost:
        fill = blend(Gdiplus::Color(0, 30, 31, 44), Theme::SurfaceElevated, v.pressed ? 1.0F : hover);
        outline = Theme::OutlineSubtle;
        ink = v.enabled ? (selected ? Theme::TextPrimary : Theme::TextSecondary) : Theme::TextMuted;
        if (selected) { fill = Theme::AccentTint; outline = Theme::Accent; }
        break;
    case Style::Danger:
        fill = blend(Gdiplus::Color(0, 74, 46, 58), Gdiplus::Color(255, 52, 30, 40), v.pressed ? 1.0F : hover);
        outline = Theme::DangerOutline;
        ink = v.enabled ? Theme::Danger : Theme::TextMuted;
        break;
    case Style::Link:
        ink = v.enabled ? blend(Theme::AccentLight, Theme::TextPrimary, hover) : Theme::TextMuted;
        radius = 6;
        break;
    case Style::Tab:
        ink = selected ? Theme::TextPrimary : (v.enabled ? blend(Theme::TextMuted, Theme::TextSecondary, hover) : Theme::TextMuted);
        break;
    case Style::Chip:
        fill = selected ? Theme::AccentTint : blend(Gdiplus::Color(0, 30, 31, 44), Theme::SurfaceElevated, hover);
        outline = selected ? Theme::Accent : Theme::OutlineSubtle;
        ink = v.enabled ? (selected ? Theme::TextPrimary : Theme::TextSecondary) : Theme::TextMuted;
        break;
    }
    if (fill.GetA() > 0) DrawingUtils::fillRoundedRectangle(g, b, radius, fill);
    if (outline.GetA() > 0) DrawingUtils::drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, radius, outline);
    if (style == Style::Tab && selected) {
        Gdiplus::SolidBrush underline(Theme::Accent);
        g.FillRectangle(&underline, b.X + 8, b.GetBottom() - 2, b.Width - 16, 2.0F);
    }
    const int fontStyle = style == Style::Link ? Gdiplus::FontStyleBold : Gdiplus::FontStyleBold;
    if (icon) {
        const float iconSize = 16;
        const float textWidth = label.empty() ? 0 : DrawingUtils::measureText(g, label, labelSize, fontStyle);
        const float gap = label.empty() ? 0.0F : 8.0F;
        const float total = iconSize + gap + textWidth;
        float x = b.X + (b.Width - total) / 2;
        if (style == Style::Link) x = b.X;
        Icons::draw(g, *icon, {x, b.Y + (b.Height - iconSize) / 2, iconSize, iconSize}, ink, 1.8F);
        if (!label.empty())
            DrawingUtils::drawText(g, label, {x + iconSize + gap, b.Y, std::max(textWidth + 4, b.GetRight() - x - iconSize - gap), b.Height}, labelSize, fontStyle, ink,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    } else {
        DrawingUtils::drawText(g, label, b, labelSize, fontStyle, ink,
            style == Style::Link ? Gdiplus::StringAlignmentNear : Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
    }
    if (v.focused) DrawingUtils::drawFocusRing(g, b, radius, style == Style::Primary ? Theme::AccentInk : Theme::AccentLight);
}

void Buttons::drawIcon(Gdiplus::Graphics& g, const Gdiplus::RectF& b, Icons::Icon icon, const ControlVisual& v,
                       bool selected, float iconSize, const Gdiplus::Color* hoverFill)
{
    const float hover = v.enabled ? std::clamp(v.hover, 0.0F, 1.0F) : 0.0F;
    const Gdiplus::Color hovered = hoverFill ? *hoverFill : Theme::SurfaceElevated;
    Gdiplus::Color fill = selected ? Theme::AccentTint : blend(Gdiplus::Color(0, hovered.GetR(), hovered.GetG(), hovered.GetB()), hovered, v.pressed ? 1.0F : hover);
    if (fill.GetA() > 0) DrawingUtils::fillRoundedRectangle(g, b, 14, fill);
    const Gdiplus::Color ink = !v.enabled ? Theme::TextMuted : selected ? Theme::AccentLight :
        (hoverFill ? blend(Theme::TextSecondary, Theme::TextPrimary, hover) : blend(Theme::TextMuted, Theme::TextPrimary, hover));
    Icons::draw(g, icon, {b.X + (b.Width - iconSize) / 2, b.Y + (b.Height - iconSize) / 2, iconSize, iconSize}, ink, 1.8F);
    if (v.focused) DrawingUtils::drawFocusRing(g, b, 14, Theme::AccentLight);
}

void Buttons::drawSwitch(Gdiplus::Graphics& g, const Gdiplus::RectF& b, bool on, const ControlVisual& v)
{
    const Gdiplus::Color track = !v.enabled ? Theme::SurfaceHover : on ? blend(Theme::Accent, Theme::AccentLight, v.hover) :
        blend(Theme::Border, Gdiplus::Color(255, 70, 70, 92), v.hover);
    DrawingUtils::fillPill(g, b, track);
    const float knob = b.Height - 6;
    const float x = on ? b.GetRight() - 3 - knob : b.X + 3;
    Gdiplus::SolidBrush brush(on ? Theme::AccentInk : Theme::TextSecondary);
    g.FillEllipse(&brush, x, b.Y + 3, knob, knob);
    if (v.focused) DrawingUtils::drawRoundedRectangle(g, {b.X - 3, b.Y - 3, b.Width + 6, b.Height + 6}, (b.Height + 6) / 2, Theme::AccentLight, 2);
}
