#include "components/PlayerProfileComponent.h"

#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

void PlayerProfileComponent::draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds) const
{
    DrawingUtils::fillRoundedRectangle(graphics, bounds, 18.0F, Theme::Surface);
    DrawingUtils::drawRoundedRectangle(graphics, bounds, 18.0F, Theme::Border);

    DrawingUtils::drawText(graphics, L"PERFIL DO JOGADOR",
                           {bounds.X + 24.0F, bounds.Y + 19.0F, 220.0F, 20.0F},
                           10.0F, Gdiplus::FontStyleBold, Theme::TextMuted);
    DrawingUtils::drawText(graphics, L"Nickname",
                           {bounds.X + 24.0F, bounds.Y + 48.0F, 180.0F, 22.0F},
                           13.0F, Gdiplus::FontStyleBold, Theme::TextPrimary);

    const auto input = inputBounds(bounds);
    DrawingUtils::fillRoundedRectangle(graphics, input, 12.0F, Theme::SurfaceElevated);
    DrawingUtils::drawRoundedRectangle(graphics, input, 12.0F, Theme::Border);

    DrawingUtils::drawText(graphics, L"Usado apenas para identificar você no jogo.",
                           {bounds.X + 24.0F, bounds.Y + 137.0F, bounds.Width - 48.0F, 18.0F},
                           10.0F, Gdiplus::FontStyleRegular, Theme::TextMuted);
}

Gdiplus::RectF PlayerProfileComponent::inputBounds(const Gdiplus::RectF& bounds) const
{
    return {bounds.X + 24.0F, bounds.Y + 76.0F, bounds.Width - 48.0F, 50.0F};
}
