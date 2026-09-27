#include "components/ProgressComponent.h"

#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <algorithm>
#include <string>

void ProgressComponent::draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                             int percent, std::wstring_view message, bool active) const
{
    DrawingUtils::fillRoundedRectangle(graphics, bounds, 18.0F, Theme::Surface);
    DrawingUtils::drawRoundedRectangle(graphics, bounds, 18.0F, Theme::Border);

    DrawingUtils::drawText(graphics, L"ATIVIDADE",
                           {bounds.X + 22.0F, bounds.Y + 18.0F, 160.0F, 18.0F},
                           10.0F, Gdiplus::FontStyleBold, Theme::TextMuted);

    const std::wstring title = active ? L"Atualização em andamento" : L"Tudo em ordem";
    DrawingUtils::drawText(graphics, title,
                           {bounds.X + 22.0F, bounds.Y + 43.0F, bounds.Width - 44.0F, 25.0F},
                           16.0F, Gdiplus::FontStyleBold, Theme::TextPrimary);
    DrawingUtils::drawText(graphics, message,
                           {bounds.X + 22.0F, bounds.Y + 70.0F, bounds.Width - 44.0F, 22.0F},
                           11.0F, Gdiplus::FontStyleRegular, Theme::TextSecondary);

    const Gdiplus::RectF track(bounds.X + 22.0F, bounds.Y + 105.0F, bounds.Width - 44.0F, 8.0F);
    DrawingUtils::fillRoundedRectangle(graphics, track, 4.0F, Theme::SurfaceElevated);

    const int clampedPercent = std::clamp(percent, 0, 100);
    const float fillWidth = active
        ? track.Width * static_cast<float>(clampedPercent) / 100.0F
        : track.Width;
    const auto fillColor = active ? Theme::Accent : Theme::Success;
    DrawingUtils::fillRoundedRectangle(graphics, {track.X, track.Y, fillWidth, track.Height},
                                       4.0F, fillColor);

    const std::wstring progressLabel = active
        ? std::to_wstring(clampedPercent) + L"%"
        : L"SEM PENDÊNCIAS";
    DrawingUtils::drawText(graphics, progressLabel,
                           {bounds.X + 22.0F, bounds.Y + 119.0F, bounds.Width - 44.0F, 17.0F},
                           9.0F, Gdiplus::FontStyleBold, active ? Theme::AccentLight : Theme::Success,
                           Gdiplus::StringAlignmentFar);
}
