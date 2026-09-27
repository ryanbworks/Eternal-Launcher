#include "utils/DrawingUtils.h"

#include "styles/Theme.h"

#include <algorithm>

std::unique_ptr<Gdiplus::GraphicsPath> DrawingUtils::createRoundedPath(
    const Gdiplus::RectF& bounds, float radius)
{
    auto path = std::make_unique<Gdiplus::GraphicsPath>();
    const float diameter = std::min(radius * 2.0F, std::min(bounds.Width, bounds.Height));
    const Gdiplus::RectF arc(bounds.X, bounds.Y, diameter, diameter);

    path->AddArc(arc, 180.0F, 90.0F);
    path->AddArc(bounds.GetRight() - diameter, bounds.Y, diameter, diameter, 270.0F, 90.0F);
    path->AddArc(bounds.GetRight() - diameter, bounds.GetBottom() - diameter,
                 diameter, diameter, 0.0F, 90.0F);
    path->AddArc(bounds.X, bounds.GetBottom() - diameter, diameter, diameter, 90.0F, 90.0F);
    path->CloseFigure();
    return path;
}

void DrawingUtils::fillRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                                        float radius, const Gdiplus::Color& color)
{
    auto path = createRoundedPath(bounds, radius);
    Gdiplus::SolidBrush brush(color);
    graphics.FillPath(&brush, path.get());
}

void DrawingUtils::drawRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                                        float radius, const Gdiplus::Color& color, float width)
{
    auto path = createRoundedPath(bounds, radius);
    Gdiplus::Pen pen(color, width);
    graphics.DrawPath(&pen, path.get());
}

void DrawingUtils::drawText(Gdiplus::Graphics& graphics, std::wstring_view text,
                            const Gdiplus::RectF& bounds, float size, int style,
                            const Gdiplus::Color& color,
                            Gdiplus::StringAlignment horizontal,
                            Gdiplus::StringAlignment vertical)
{
    Gdiplus::Font font(Theme::FontFamily, size, style, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(color);
    Gdiplus::StringFormat format;
    format.SetAlignment(horizontal);
    format.SetLineAlignment(vertical);
    format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    graphics.DrawString(text.data(), static_cast<INT>(text.size()), &font, bounds, &format, &brush);
}

Gdiplus::Color DrawingUtils::colorForTone(StatusTone tone)
{
    switch (tone)
    {
    case StatusTone::Success:
        return Theme::Success;
    case StatusTone::Information:
        return Theme::Information;
    case StatusTone::Warning:
        return Theme::Warning;
    case StatusTone::Error:
        return Theme::Error;
    default:
        return Theme::TextMuted;
    }
}

bool DrawingUtils::contains(const Gdiplus::RectF& bounds, float x, float y)
{
    return x >= bounds.X && x <= bounds.GetRight() &&
           y >= bounds.Y && y <= bounds.GetBottom();
}
