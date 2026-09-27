#include "components/HeaderComponent.h"

#include "config/LauncherConfig.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <string>

void HeaderComponent::draw(Gdiplus::Graphics& graphics, float width) const
{
    const Gdiplus::RectF logoBounds(32.0F, 24.0F, 52.0F, 52.0F);
    auto logoPath = DrawingUtils::createRoundedPath(logoBounds, 15.0F);
    Gdiplus::LinearGradientBrush logoBrush(
        logoBounds, Theme::AccentLight, Theme::Accent, Gdiplus::LinearGradientModeForwardDiagonal);
    graphics.FillPath(&logoBrush, logoPath.get());

    // A letra estilizada funciona como marca provisória sem depender de um bitmap.
    DrawingUtils::drawText(graphics, L"E", logoBounds, 27.0F, Gdiplus::FontStyleBold,
                           Theme::TextPrimary, Gdiplus::StringAlignmentCenter,
                           Gdiplus::StringAlignmentCenter);

    DrawingUtils::drawText(graphics, L"ETERNAL", {100.0F, 25.0F, 250.0F, 30.0F},
                           24.0F, Gdiplus::FontStyleBold, Theme::TextPrimary);
    DrawingUtils::drawText(graphics, L"PRIVATE MODDED EXPERIENCE", {101.0F, 56.0F, 300.0F, 20.0F},
                           10.0F, Gdiplus::FontStyleBold, Theme::TextMuted);

    const std::wstring versionText = L"VERSÃO  " + std::wstring(LauncherConfig::LauncherVersion);
    const Gdiplus::RectF versionBounds(width - 230.0F, 31.0F, 138.0F, 36.0F);
    DrawingUtils::fillRoundedRectangle(graphics, versionBounds, 18.0F, Theme::Surface);
    DrawingUtils::drawRoundedRectangle(graphics, versionBounds, 18.0F, Theme::Border);
    DrawingUtils::drawText(graphics, versionText, versionBounds, 10.0F, Gdiplus::FontStyleBold,
                           Theme::TextSecondary, Gdiplus::StringAlignmentCenter,
                           Gdiplus::StringAlignmentCenter);

    const auto settings = settingsBounds(width);
    DrawingUtils::fillRoundedRectangle(graphics, settings, 18.0F, Theme::Surface);
    DrawingUtils::drawRoundedRectangle(graphics, settings, 18.0F, Theme::Border);

    // Engrenagem minimalista: círculos concêntricos preservam legibilidade em qualquer DPI.
    Gdiplus::Pen gearPen(Theme::TextSecondary, 2.0F);
    graphics.DrawEllipse(&gearPen, settings.X + 11.0F, settings.Y + 11.0F, 14.0F, 14.0F);
    graphics.DrawEllipse(&gearPen, settings.X + 16.0F, settings.Y + 16.0F, 4.0F, 4.0F);
}

Gdiplus::RectF HeaderComponent::settingsBounds(float width) const
{
    return {width - 76.0F, 31.0F, 36.0F, 36.0F};
}
