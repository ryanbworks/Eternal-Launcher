#include "components/StatusPanelComponent.h"

#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <algorithm>

void StatusPanelComponent::draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                                const std::vector<StatusCardData>& cards) const
{
    DrawingUtils::drawText(graphics, L"VISÃO GERAL", {bounds.X, bounds.Y, 160.0F, 20.0F},
                           11.0F, Gdiplus::FontStyleBold, Theme::TextMuted);

    if (cards.empty())
    {
        return;
    }

    constexpr float gap = 12.0F;
    const float cardWidth = (bounds.Width - gap * 2.0F) / 3.0F;
    const std::size_t count = std::min<std::size_t>(cards.size(), 3);

    for (std::size_t index = 0; index < count; ++index)
    {
        const auto& card = cards[index];
        const Gdiplus::RectF cardBounds(
            bounds.X + static_cast<float>(index) * (cardWidth + gap),
            bounds.Y + 26.0F,
            cardWidth,
            bounds.Height - 26.0F);

        DrawingUtils::fillRoundedRectangle(graphics, cardBounds, 16.0F, Theme::Surface);
        DrawingUtils::drawRoundedRectangle(graphics, cardBounds, 16.0F, Theme::Border);

        const auto toneColor = DrawingUtils::colorForTone(card.tone);
        Gdiplus::SolidBrush indicatorBrush(toneColor);
        graphics.FillEllipse(&indicatorBrush, cardBounds.X + 18.0F, cardBounds.Y + 20.0F, 8.0F, 8.0F);

        DrawingUtils::drawText(graphics, card.label,
                               {cardBounds.X + 36.0F, cardBounds.Y + 14.0F,
                                cardBounds.Width - 50.0F, 20.0F},
                               10.0F, Gdiplus::FontStyleBold, Theme::TextMuted);
        DrawingUtils::drawText(graphics, card.value,
                               {cardBounds.X + 18.0F, cardBounds.Y + 43.0F,
                                cardBounds.Width - 36.0F, 27.0F},
                               18.0F, Gdiplus::FontStyleBold, Theme::TextPrimary);
        DrawingUtils::drawText(graphics, card.detail,
                               {cardBounds.X + 18.0F, cardBounds.Y + 74.0F,
                                cardBounds.Width - 36.0F, 20.0F},
                               11.0F, Gdiplus::FontStyleRegular, Theme::TextSecondary);
    }
}
