#include "components/ProgressComponent.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"
#include <algorithm>
#include <cmath>
#include <string>

void ProgressComponent::draw(Gdiplus::Graphics& g,const Gdiplus::RectF& b,int percent,
    std::wstring_view message,bool active,bool healthy,float displayedPercent,float phase,
    std::wstring_view title,bool recoveryLink) const
{
    Gdiplus::Pen rule(Theme::Border); g.DrawLine(&rule,b.X,b.Y,b.GetRight(),b.Y);
    DrawingUtils::drawText(g,!title.empty() ? title : active ? L"Preparando seu mundo" : healthy ? L"Atividade" : L"Atenção necessária",
        {b.X,b.Y+17,b.Width-48,24},13,Gdiplus::FontStyleBold,Theme::TextPrimary);
    if(active) DrawingUtils::drawText(g,std::to_wstring(std::clamp(percent,0,100))+L"%",
        {b.GetRight()-45,b.Y+17,45,24},12,Gdiplus::FontStyleBold,Theme::AccentLight,Gdiplus::StringAlignmentFar);
    DrawingUtils::drawText(g,message,{b.X,b.Y+48,b.Width,recoveryLink ? 38.0F : 49.0F},11,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    if(recoveryLink) DrawingUtils::drawText(g,L"Baixar launcher",{b.X,b.Y+88,b.Width,24},11,
        Gdiplus::FontStyleUnderline,Theme::AccentLight);
    if(active) {
        const Gdiplus::RectF track(b.X,b.Y+105,b.Width,4);
        DrawingUtils::fillRoundedRectangle(g,track,2,Theme::SurfaceHover);
        const float value=displayedPercent<0 ? static_cast<float>(percent) : displayedPercent;
        if(percent==0) {
            const auto saved=g.Save(); g.SetClip(track);
            const float x=track.X+(std::fmod(phase,1.0F)*1.35F-0.35F)*track.Width;
            DrawingUtils::fillRoundedRectangle(g,{x,track.Y,track.Width*0.35F,4},2,Theme::Accent); g.Restore(saved);
        } else DrawingUtils::fillRoundedRectangle(g,{track.X,track.Y,track.Width*std::clamp(value,0.0F,100.0F)/100,4},2,Theme::Accent);
    }
}
