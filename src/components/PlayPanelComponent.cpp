#include "components/PlayPanelComponent.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"
#include <algorithm>

void PlayPanelComponent::draw(Gdiplus::Graphics& g,const Gdiplus::RectF& b,
    const LauncherViewState& state,float hover,bool focused,bool pressed,float compression,float sweep,float pulse,
    float light,float phase,bool effects) const
{
    const bool error=isLauncherError(state.operation);
    Gdiplus::SolidBrush dot(error ? Theme::Error :
        state.operation==LauncherOperation::NeedsInstallation ? Theme::Warning :
        state.playEnabled ? Theme::Success : Theme::Accent);
    g.FillEllipse(&dot,b.X,b.Y+9,7.0F,7.0F);
    DrawingUtils::drawText(g,state.headline,{b.X+16,b.Y,b.Width-16,48},17,Gdiplus::FontStyleBold,Theme::TextPrimary);
    DrawingUtils::drawText(g,state.operationMessage,{b.X,b.Y+54,b.Width,58},12,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    auto button=buttonBounds(b);
    const float inset=3*std::clamp(compression,0.0F,1.0F);
    button={button.X+inset,button.Y+inset,button.Width-2*inset,button.Height-2*inset};
    auto color=state.playEnabled ? DrawingUtils::blend(Theme::Accent,Theme::AccentLight,hover) : Theme::SurfaceHover;
    if(pressed && state.playEnabled) color=Theme::AccentLight;
    DrawingUtils::fillRoundedRectangle(g,button,Theme::ControlRadius,color);
    if(sweep<1 || pulse>0 || (effects && hover>.005F)) {
        const auto saved=g.Save();const auto clip=DrawingUtils::createRoundedPath(button,Theme::ControlRadius);g.SetClip(clip.get(),Gdiplus::CombineModeIntersect);
        if(state.playEnabled && sweep<1) {
            const float x=button.X-70+(button.Width+140)*sweep;
            const Gdiplus::PointF beam[]={{x-28,button.Y},{x+60,button.Y},{x+32,button.GetBottom()},{x-56,button.GetBottom()}};
            Gdiplus::LinearGradientBrush sheen(Gdiplus::RectF{x-56,button.Y,116,button.Height},Gdiplus::Color(0,255,255,255),
                Gdiplus::Color(0,255,255,255),Gdiplus::LinearGradientModeHorizontal);
            const Gdiplus::Color shades[]={{0,255,255,255},{95,255,255,255},{0,255,255,255}};
            const Gdiplus::REAL stops[]={0,.5F,1};sheen.SetInterpolationColors(shades,stops,3);g.FillPolygon(&sheen,beam,4);
        }
        if(effects && state.playEnabled && hover>.005F) {
            const float x=button.X+button.Width*std::clamp(light,0.0F,1.0F);
            Gdiplus::LinearGradientBrush spotlight(Gdiplus::RectF{x-75,button.Y,150,button.Height},Gdiplus::Color(0,255,255,255),
                Gdiplus::Color(0,255,255,255),Gdiplus::LinearGradientModeHorizontal);
            const Gdiplus::Color shades[]={{0,255,255,255},{static_cast<BYTE>(75*hover),255,255,255},{0,255,255,255}};
            const Gdiplus::REAL stops[]={0,.5F,1};spotlight.SetInterpolationColors(shades,stops,3);
            g.FillRectangle(&spotlight,Gdiplus::RectF{x-75,button.Y,150,button.Height});
            Gdiplus::Pen edge(Gdiplus::Color(static_cast<BYTE>(175*hover),255,255,255),1.6F);
            const float head=button.X-60+(button.Width+120)*phase;
            g.DrawLine(&edge,head,button.Y+1,head+60,button.Y+1);
        }
        if(pulse>0) {
            const float radius=(1-pulse)*button.Width*.7F;
            Gdiplus::SolidBrush ripple(Gdiplus::Color(static_cast<BYTE>(65*pulse),255,255,255));
            g.FillEllipse(&ripple,button.X+button.Width/2-radius,button.Y+button.Height/2-radius,2*radius,2*radius);
        }
        g.Restore(saved);
    }
    const auto ink=state.playEnabled ? Theme::AccentInk : Theme::TextSecondary;
    const bool play=state.operation==LauncherOperation::ReadyToPlay;
    const auto label=state.primaryActionLabel.empty() ? L"JOGAR AGORA" : state.primaryActionLabel;
    DrawingUtils::drawText(g,label,{button.X+(play ? 23.0F : 0),button.Y,button.Width-(play ? 23.0F : 0),button.Height},
        13,Gdiplus::FontStyleBold,ink,Gdiplus::StringAlignmentCenter,Gdiplus::StringAlignmentCenter);
    if(play) {
        const float x=button.X+58+10*hover,y=button.Y+button.Height/2;
        if(effects && hover>.005F) {
            Gdiplus::Pen wake(Gdiplus::Color(static_cast<BYTE>(110*hover),36,18,68),1.5F);
            g.DrawLine(&wake,x-12,y-5,x-7,y);g.DrawLine(&wake,x-7,y,x-12,y+5);
        }
        const Gdiplus::PointF points[]={{x,y-7},{x+10,y},{x,y+7}};
        Gdiplus::SolidBrush triangle(ink); g.FillPolygon(&triangle,points,3);
    }
    if(focused) DrawingUtils::drawRoundedRectangle(g,{button.X+3,button.Y+3,button.Width-6,button.Height-6},7,Theme::AccentInk,2);
}
Gdiplus::RectF PlayPanelComponent::buttonBounds(const Gdiplus::RectF& b) const {return {b.X,b.Y+128,b.Width,54};}
