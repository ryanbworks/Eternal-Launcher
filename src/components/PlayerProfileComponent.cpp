#include "components/PlayerProfileComponent.h"

#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

void PlayerProfileComponent::draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                                  bool original, std::wstring_view playerName) const
{
    DrawingUtils::fillRoundedRectangle(graphics, bounds, 18.0F, Theme::Surface);
    DrawingUtils::drawRoundedRectangle(graphics, bounds, 18.0F, Theme::Border);

    DrawingUtils::drawText(graphics, L"PERFIL DO JOGADOR",
                           {bounds.X + 24.0F, bounds.Y + 19.0F, 220.0F, 20.0F},
                           10.0F, Gdiplus::FontStyleBold, Theme::TextMuted);
    const auto offline = offlineTabBounds(bounds);
    const auto microsoft = originalTabBounds(bounds);
    DrawingUtils::fillRoundedRectangle(graphics, offline, 8.0F,
        original ? Theme::SurfaceElevated : Theme::Border);
    DrawingUtils::fillRoundedRectangle(graphics, microsoft, 8.0F,
        original ? Theme::Border : Theme::SurfaceElevated);
    DrawingUtils::drawText(graphics, L"Nickname local", offline, 11.0F,
                           Gdiplus::FontStyleBold, Theme::TextPrimary);
    DrawingUtils::drawText(graphics, L"Original", microsoft, 11.0F,
                           Gdiplus::FontStyleBold, Theme::TextPrimary);

    const auto input = inputBounds(bounds);
    DrawingUtils::fillRoundedRectangle(graphics, input, 12.0F, Theme::SurfaceElevated);
    DrawingUtils::drawRoundedRectangle(graphics, input, 12.0F, Theme::Border);
    if (original)
    {
        DrawingUtils::drawText(graphics,
            playerName.empty() ? L"Entrar com Microsoft ao clicar em Jogar"
                               : std::wstring(L"Conectado: ") + std::wstring(playerName),
            {input.X + 14.0F, input.Y + 11.0F, input.Width - 28.0F, 28.0F},
            12.0F, Gdiplus::FontStyleBold, Theme::TextPrimary);
    }

    DrawingUtils::drawText(graphics, original ? L"Conta Minecraft: Java Edition original."
                                              : L"Usado apenas para identificar você no jogo.",
                           {bounds.X + 24.0F, bounds.Y + 137.0F, bounds.Width - 48.0F, 18.0F},
                           10.0F, Gdiplus::FontStyleRegular, Theme::TextMuted);
}

Gdiplus::RectF PlayerProfileComponent::offlineTabBounds(const Gdiplus::RectF& bounds) const
{
    return {bounds.X + 24.0F, bounds.Y + 46.0F, 158.0F, 27.0F};
}

Gdiplus::RectF PlayerProfileComponent::originalTabBounds(const Gdiplus::RectF& bounds) const
{
    return {bounds.X + 190.0F, bounds.Y + 46.0F, 158.0F, 27.0F};
}

Gdiplus::RectF PlayerProfileComponent::inputBounds(const Gdiplus::RectF& bounds) const
{
    return {bounds.X + 24.0F, bounds.Y + 76.0F, bounds.Width - 48.0F, 50.0F};
}
