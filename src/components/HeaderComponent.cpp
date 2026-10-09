#include "components/HeaderComponent.h"
#include "config/LauncherConfig.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"
#include <cmath>
#include <cstring>
#include <string>
#include <windows.h>

HeaderComponent::HeaderComponent()
{
    loadLogo();
}

HeaderComponent::~HeaderComponent()
{
    logo_.reset();
    if (logoStream_) logoStream_->Release();
}

void HeaderComponent::loadLogo()
{
    const auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(105), RT_RCDATA);
    if (!resource) return;
    const auto loaded = LoadResource(nullptr, resource);
    const auto data = loaded ? LockResource(loaded) : nullptr;
    const auto size = loaded ? SizeofResource(nullptr, resource) : 0;
    if (!data || !size) return;

    const auto memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return;
    const auto target = GlobalLock(memory);
    if (!target) {
        GlobalFree(memory);
        return;
    }
    std::memcpy(target, data, size);
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return;
    }
    auto bitmap = std::unique_ptr<Gdiplus::Bitmap>(Gdiplus::Bitmap::FromStream(stream));
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
        if (bitmap) bitmap.reset();
        stream->Release();
        return;
    }
    logoStream_ = stream;
    logo_ = std::move(bitmap);
}

void HeaderComponent::draw(Gdiplus::Graphics& g, float width, float hover, bool focused, bool showSettings) const
{
    if (logo_) {
        DrawingUtils::drawCachedImage(g,*logo_,scaledLogo_,{35,24,46,46},
            {0,0,static_cast<float>(logo_->GetWidth()),static_cast<float>(logo_->GetHeight())});
    } else {
        DrawingUtils::drawLogo(g, {36, 25, 43, 43}, Theme::Accent);
    }
    DrawingUtils::drawDisplayText(g,L"eternal",{92,22,145,38},26,Theme::TextPrimary);
    DrawingUtils::drawText(g,L"LAUNCHER",{94,59,140,18},10,Gdiplus::FontStyleBold,Theme::TextMuted);
    Gdiplus::Pen rule(Theme::Border);
    g.DrawLine(&rule,248.0F,33.0F,248.0F,66.0F);
    DrawingUtils::drawText(g,L"Minecraft",{271,34,150,28},15,Gdiplus::FontStyleBold,Theme::TextPrimary);
    DrawingUtils::drawText(g,L"Java Edition",{384,37,150,22},12,Gdiplus::FontStyleRegular,Theme::TextMuted);
    DrawingUtils::drawText(g,L"v" + std::wstring(LauncherConfig::LauncherVersion),
        {width-238,37,133,22},12,Gdiplus::FontStyleRegular,Theme::TextMuted,Gdiplus::StringAlignmentFar);
    if (showSettings) {
    const auto b = settingsBounds(width);
    DrawingUtils::fillRoundedRectangle(g,b,Theme::ControlRadius,
        DrawingUtils::blend(Theme::Background,Theme::SurfaceHover,hover));
    const float cx = b.X + b.Width/2, cy = b.Y + b.Height/2;
    const auto gearState=g.Save();
    g.TranslateTransform(cx,cy);g.RotateTransform(60*hover);g.TranslateTransform(-cx,-cy);
    Gdiplus::Pen gear(Theme::TextSecondary,1.7F);
    g.DrawEllipse(&gear,cx-7,cy-7,14.0F,14.0F);
    g.DrawEllipse(&gear,cx-2.5F,cy-2.5F,5.0F,5.0F);
    for(int i=0;i<8;++i) {
        const float a = static_cast<float>(i)*3.14159265F/4;
        g.DrawLine(&gear,cx+std::cos(a)*7,cy+std::sin(a)*7,cx+std::cos(a)*10,cy+std::sin(a)*10);
    }
    g.Restore(gearState);
    if(focused) DrawingUtils::drawRoundedRectangle(g,b,Theme::ControlRadius,Theme::AccentLight,2);
    }
    g.DrawLine(&rule,36.0F,88.0F,width-36,88.0F);
}
Gdiplus::RectF HeaderComponent::settingsBounds(float width) const { return {width-80,28,44,44}; }
