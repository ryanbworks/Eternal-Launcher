#include "components/PlayerProfileComponent.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

void PlayerProfileComponent::draw(Gdiplus::Graphics& g,const Gdiplus::RectF& b,
    bool original,std::wstring_view name,float offlineHover,float originalHover,int focus,bool enabled,float selection) const
{
    DrawingUtils::drawText(g,L"Seu acesso",{b.X,b.Y,b.Width,30},22,Gdiplus::FontStyleBold,Theme::TextPrimary);
    DrawingUtils::drawText(g,L"Escolha como entrar no mundo.",{b.X,b.Y+35,b.Width,22},12,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    DrawingUtils::fillRoundedRectangle(g,{b.X,b.Y+70,b.Width,40},Theme::ControlRadius,Theme::Background);
    const auto first=offlineTabBounds(b),second=originalTabBounds(b);
    DrawingUtils::fillRoundedRectangle(g,first,8,DrawingUtils::blend(Theme::Background,Theme::SurfaceElevated,offlineHover));
    DrawingUtils::fillRoundedRectangle(g,second,8,DrawingUtils::blend(Theme::Background,Theme::SurfaceElevated,originalHover));
    if(selection<0) selection=original ? 1.0F : 0.0F;
    DrawingUtils::fillRoundedRectangle(g,{first.X+(second.X-first.X)*selection,first.Y,
        first.Width+(second.Width-first.Width)*selection,first.Height},8,Theme::SurfaceHover);
    const auto tab=[&](const Gdiplus::RectF& r,std::wstring_view label,bool selected,bool focused) {
        DrawingUtils::drawText(g,label,r,12,Gdiplus::FontStyleBold,
            enabled ? (selected ? Theme::AccentLight : Theme::TextSecondary) : Theme::TextMuted,
            Gdiplus::StringAlignmentCenter,Gdiplus::StringAlignmentCenter);
        if(focused) DrawingUtils::drawRoundedRectangle(g,r,8,Theme::AccentLight,2);
    };
    tab(first,L"Nickname local",!original,focus==1);
    tab(second,L"Original",original,focus==2);
    DrawingUtils::drawText(g,original ? L"Conta Microsoft" : L"Nickname",{b.X,b.Y+126,b.Width,21},12,Gdiplus::FontStyleBold,Theme::TextSecondary);
    const auto input=inputBounds(b);
    DrawingUtils::fillRoundedRectangle(g,input,Theme::ControlRadius,Theme::SurfaceElevated);
    DrawingUtils::drawRoundedRectangle(g,input,Theme::ControlRadius,focus==3 ? Theme::AccentLight : Theme::Border,focus==3 ? 2.0F : 1.0F);
    if(original) DrawingUtils::drawText(g,name.empty() ? L"Entrar ao clicar em Jogar" : std::wstring(name),
        {input.X+14,input.Y,input.Width-28,input.Height},13,Gdiplus::FontStyleRegular,Theme::TextPrimary,
        Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
    DrawingUtils::drawText(g,original ? L"Requer Minecraft: Java Edition." : L"3 a 16 letras, números ou _",
        {b.X,b.Y+209,b.Width,20},11,Gdiplus::FontStyleRegular,Theme::TextMuted);
}
Gdiplus::RectF PlayerProfileComponent::offlineTabBounds(const Gdiplus::RectF& b) const {
    return {b.X+4,b.Y+74,(b.Width-8)*0.56F,32};
}
Gdiplus::RectF PlayerProfileComponent::originalTabBounds(const Gdiplus::RectF& b) const {
    const auto first=offlineTabBounds(b); return {first.GetRight(),first.Y,(b.Width-8)*0.44F,32};
}
Gdiplus::RectF PlayerProfileComponent::inputBounds(const Gdiplus::RectF& b) const {return {b.X,b.Y+153,b.Width,44};}
