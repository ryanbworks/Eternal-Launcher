#include "components/NewsPanelComponent.h"

#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

void NewsPanelComponent::draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                              const NewsItem& news) const
{
    DrawingUtils::fillRoundedRectangle(graphics, bounds, 18.0F, Theme::Surface);
    DrawingUtils::drawRoundedRectangle(graphics, bounds, 18.0F, Theme::Border);

    // O marcador vertical diferencia notícias sem adicionar ruído visual ao painel.
    DrawingUtils::fillRoundedRectangle(
        graphics, {bounds.X + 18.0F, bounds.Y + 20.0F, 4.0F, bounds.Height - 40.0F},
        2.0F, Theme::Accent);

    DrawingUtils::drawText(graphics, news.category,
                           {bounds.X + 40.0F, bounds.Y + 20.0F, 240.0F, 18.0F},
                           10.0F, Gdiplus::FontStyleBold, Theme::AccentLight);
    DrawingUtils::drawText(graphics, news.date,
                           {bounds.GetRight() - 105.0F, bounds.Y + 20.0F, 78.0F, 18.0F},
                           10.0F, Gdiplus::FontStyleRegular, Theme::TextMuted,
                           Gdiplus::StringAlignmentFar);
    DrawingUtils::drawText(graphics, news.title,
                           {bounds.X + 40.0F, bounds.Y + 46.0F, bounds.Width - 80.0F, 28.0F},
                           18.0F, Gdiplus::FontStyleBold, Theme::TextPrimary);
    DrawingUtils::drawText(graphics, news.description,
                           {bounds.X + 40.0F, bounds.Y + 81.0F, bounds.Width - 80.0F, 44.0F},
                           12.0F, Gdiplus::FontStyleRegular, Theme::TextSecondary);
}
