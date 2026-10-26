#include "components/HeroComponent.h"
#include "config/LauncherConfig.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <string>

HeroComponent::HeroComponent()
{
    const auto resource=FindResource(nullptr,MAKEINTRESOURCE(102),RT_RCDATA);
    if(!resource) return;
    const auto loaded=LoadResource(nullptr,resource);
    const auto data=LockResource(loaded);
    const auto size=SizeofResource(nullptr,resource);
    if(!data || !size) return;
    const auto memory=GlobalAlloc(GMEM_MOVEABLE,size);
    if(!memory) return;
    const auto target=GlobalLock(memory);
    if(!target) { GlobalFree(memory); return; }
    std::memcpy(target,data,size); GlobalUnlock(memory);
    if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&stream_))) {GlobalFree(memory);return;}
    artwork_.reset(Gdiplus::Bitmap::FromStream(stream_));
    if(!artwork_ || artwork_->GetLastStatus()!=Gdiplus::Ok) artwork_.reset();
}
HeroComponent::~HeroComponent() {artwork_.reset(); if(stream_) stream_->Release();}

void HeroComponent::draw(Gdiplus::Graphics& g,const Gdiplus::RectF& b,float reveal,float energy,float phase,float drift,
    const std::vector<std::wstring>& chips) const
{
    auto path=DrawingUtils::createRoundedPath(b,Theme::PanelRadius);
    const auto saved=g.Save(); g.SetClip(path.get());
    Gdiplus::SolidBrush base(Theme::Surface); g.FillRectangle(&base,b);
    if(artwork_) {
        // The artwork opens around the portal side; copy and controls stay visible throughout.
        reveal=std::clamp(reveal,0.0F,1.0F);
        const float arrival=1-(1-reveal)*(1-reveal);
        const float left=b.X+b.Width*.78F*(1-arrival);
        const float right=b.GetRight()-b.Width*.09F*(1-arrival);
        const auto imageState=g.Save();
        g.SetClip(Gdiplus::RectF(left,b.Y,right-left,b.Height),Gdiplus::CombineModeIntersect);
        const float sw=static_cast<float>(artwork_->GetWidth()),sh=static_cast<float>(artwork_->GetHeight());
        const float scale=std::max(b.Width/sw,b.Height/sh);
        const float cropH=b.Height/scale,cropW=b.Width/scale;
        DrawingUtils::drawCachedImage(g,*artwork_,scaledArtwork_,b,{(sw-cropW)/2,(sh-cropH)/2,cropW,cropH});
        g.Restore(imageState);
        if(reveal<1) {
            Gdiplus::Pen aperture(Gdiplus::Color(static_cast<BYTE>(100*(1-arrival)),201,182,255),1);
            g.DrawLine(&aperture,left,b.Y,left,b.GetBottom());
            g.DrawLine(&aperture,right,b.Y,right,b.GetBottom());
        }
    }
    Gdiplus::LinearGradientBrush shade(b,Gdiplus::Color(235,14,15,27),Gdiplus::Color(0,14,15,27),Gdiplus::LinearGradientModeHorizontal);
    const Gdiplus::Color colors[]={{120,14,15,27},{40,14,15,27},{0,14,15,27}};
    const Gdiplus::REAL stops[]={0,0.48F,1};
    shade.SetInterpolationColors(colors,stops,3); g.FillRectangle(&shade,b);
    if(energy>.005F) {
        // Confine the authored energy to the actual portal side, clear of operational copy.
        const float cx=b.X+b.Width*.844F,cy=b.Y+b.Height*.62F;
        const float turn=phase*360;
        for(int orbit=0;orbit<2;++orbit) {
            const float spread=static_cast<float>(orbit)*14;
            Gdiplus::Pen ribbon(Gdiplus::Color(static_cast<BYTE>(180*energy),201,182,255),1.8F);
            g.DrawArc(&ribbon,cx-30-spread,cy-47-spread,60+2*spread,94+2*spread,
                turn*(orbit==0 ? 1 : -1)+static_cast<float>(orbit)*180,125);
        }
        for(int i=0;i<24;++i) {
            const float life=std::fmod(phase+static_cast<float>(i)*.0618F,1.0F);
            const float angle=static_cast<float>(i)*2.39996F;
            const float x=cx+std::sin(angle+life*2)*(.25F+life)*66;
            const float y=cy+42-life*145;
            const float visibility=std::sin(life*3.14159265F)*energy;
            Gdiplus::SolidBrush spark(Gdiplus::Color(static_cast<BYTE>(220*visibility),218,198,255));
            const float size=2+static_cast<float>(i%3);
            g.FillEllipse(&spark,x,y,size,size);
            if(i%4==0) {
                Gdiplus::Pen trail(Gdiplus::Color(static_cast<BYTE>(100*visibility),165,130,255),1);
                g.DrawLine(&trail,x+size/2,y+size,x+size/2,y+size+9);
            }
        }
        // Distant dust, middle embers and sparse foreground sparks have distinct speeds/sizes.
        // The copy occupies the left half; keep all atmospheric particles on the artwork side.
        for(int i=0;i<40;++i) {
            const int layer=i%3;
            const float seed=static_cast<float>(i);
            const float life=std::fmod(drift*(1+static_cast<float>(layer)) + seed*.618034F,1.0F);
            const float lane=std::fmod(seed*.414214F,1.0F);
            const float x=b.X+b.Width*(.57F+.40F*lane)+std::sin(life*6.283185F+seed)*static_cast<float>(3+layer*3);
            const float y=b.Y+b.Height*(1-life);
            const float visibility=std::sin(life*3.14159265F)*(.42F+.58F*energy);
            const float size=1.2F+static_cast<float>(layer)*1.25F;
            const BYTE alpha=static_cast<BYTE>((100+layer*50)*visibility);
            if(layer==2) {
                Gdiplus::SolidBrush halo(Gdiplus::Color(static_cast<BYTE>(alpha/5),174,137,255));
                g.FillEllipse(&halo,x-2,y-2,size+4,size+4);
                Gdiplus::Pen trail(Gdiplus::Color(static_cast<BYTE>(alpha/2),194,169,255),1);
                g.DrawLine(&trail,x+size/2,y+size,x+size/2,y+size+6);
            }
            Gdiplus::SolidBrush dust(Gdiplus::Color(alpha,224,204,255));g.FillEllipse(&dust,x,y,size,size);
        }
    }
    g.Restore(saved);
    // Glass panel: a translucent dark plate keeps the copy legible over any part of the art.
    const Gdiplus::RectF glass{b.X+20,b.GetBottom()-172,440,152};
    DrawingUtils::fillRoundedRectangle(g,glass,16,Theme::Glass);
    DrawingUtils::drawRoundedRectangle(g,{glass.X+.5F,glass.Y+.5F,glass.Width-1,glass.Height-1},16,Theme::GlassOutline);
    DrawingUtils::drawText(g,L"ETERNAL REALMS",{glass.X+24,glass.Y+20,300,16},11,Gdiplus::FontStyleBold,Theme::AccentLight);
    DrawingUtils::drawDisplayText(g,L"Sua próxima aventura\ncomeça aqui.",{glass.X+24,glass.Y+40,glass.Width-40,74},28,Theme::TextPrimary);
    float x=glass.X+24;
    for(const auto& chip:chips) {
        if(chip.empty()) continue;
        const float w=DrawingUtils::measureText(g,chip,12,Gdiplus::FontStyleBold)+22;
        const Gdiplus::RectF pill{x,glass.GetBottom()-38,w,24};
        DrawingUtils::fillPill(g,pill,Gdiplus::Color(22,255,255,255));
        DrawingUtils::drawText(g,chip,pill,12,Gdiplus::FontStyleBold,Gdiplus::Color(255,230,231,239),
            Gdiplus::StringAlignmentCenter,Gdiplus::StringAlignmentCenter);
        x+=w+8;
    }
}


void HeroComponent::drawThumbnail(Gdiplus::Graphics& g,const Gdiplus::RectF& b,std::size_t slot,float focusX,float focusY) const
{
    if(!artwork_ || slot>=thumbnails_.size()) {
        Gdiplus::SolidBrush fill(Theme::SurfaceHover);g.FillRectangle(&fill,b);return;
    }
    const float sw=static_cast<float>(artwork_->GetWidth()),sh=static_cast<float>(artwork_->GetHeight());
    float cropW=sw*.45F,cropH=cropW*b.Height/b.Width;
    if(cropH>sh) {cropH=sh;cropW=cropH*b.Width/b.Height;}
    const float x=std::clamp(sw*focusX-cropW/2,0.0F,sw-cropW),y=std::clamp(sh*focusY-cropH/2,0.0F,sh-cropH);
    DrawingUtils::drawCachedImage(g,*artwork_,thumbnails_[slot],b,{x,y,cropW,cropH});
}
