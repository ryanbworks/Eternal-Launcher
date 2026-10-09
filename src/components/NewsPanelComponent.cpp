#include "components/NewsPanelComponent.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

void NewsPanelComponent::draw(Gdiplus::Graphics& g,const Gdiplus::RectF& b,const NewsItem& news) const
{
    Gdiplus::Pen line(Theme::Border);
    g.DrawLine(&line,b.X,b.Y,b.GetRight(),b.Y);
    DrawingUtils::drawText(g,news.title,{b.X,b.Y+16,b.Width-180,25},15,Gdiplus::FontStyleBold,Theme::TextPrimary);
    DrawingUtils::drawText(g,L"Notícia de demonstração",{b.GetRight()-172,b.Y+18,172,20},11,
        Gdiplus::FontStyleRegular,Theme::TextMuted,Gdiplus::StringAlignmentFar);
    DrawingUtils::drawText(g,news.description,{b.X,b.Y+44,b.Width-10,23},12,Gdiplus::FontStyleRegular,Theme::TextSecondary);
}
