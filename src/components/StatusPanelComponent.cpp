#include "components/StatusPanelComponent.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"
#include <algorithm>

void StatusPanelComponent::draw(Gdiplus::Graphics& g,const Gdiplus::RectF& b,
    const std::vector<StatusCardData>& cards) const
{
    DrawingUtils::drawText(g,L"Sua instalação",{b.X,b.Y,b.Width,26},18,Gdiplus::FontStyleBold,Theme::TextPrimary);
    const float width = b.Width/3;
    Gdiplus::Pen divider(Theme::Border);
    for(std::size_t i=0;i<std::min<std::size_t>(cards.size(),3);++i) {
        const auto& card=cards[i];
        const float x=b.X+static_cast<float>(i)*width;
        if(i!=0) g.DrawLine(&divider,x-14,b.Y+38,x-14,b.GetBottom());
        Gdiplus::SolidBrush dot(DrawingUtils::colorForTone(card.tone));
        g.FillEllipse(&dot,x,b.Y+43,6.0F,6.0F);
        DrawingUtils::drawText(g,card.label,{x+14,b.Y+38,width-34,20},10,Gdiplus::FontStyleBold,Theme::TextMuted);
        DrawingUtils::drawText(g,card.value,{x,b.Y+61,width-30,26},17,Gdiplus::FontStyleBold,Theme::TextPrimary);
        DrawingUtils::drawText(g,card.detail,{x,b.Y+92,width-30,35},11,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    }
}
