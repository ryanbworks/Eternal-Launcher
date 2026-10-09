#include "utils/DrawingUtils.h"

#include "styles/Theme.h"

#include <algorithm>
#include <cmath>

namespace { const Gdiplus::FontFamily* DisplayFamily = nullptr; }

DrawingUtils::DisplayTypeface::DisplayTypeface()
{
    const auto resource = FindResource(nullptr, MAKEINTRESOURCE(103), RT_RCDATA);
    if (!resource) return;
    const auto data = LockResource(LoadResource(nullptr, resource));
    const auto size = SizeofResource(nullptr, resource);
    if (!data || !size || collection_.AddMemoryFont(data, static_cast<INT>(size)) != Gdiplus::Ok) return;
    INT found = 0;
    if (collection_.GetFamilies(1, &family_, &found) == Gdiplus::Ok && found == 1) DisplayFamily = &family_;
}

DrawingUtils::DisplayTypeface::~DisplayTypeface() { DisplayFamily = nullptr; }

void DrawingUtils::drawCachedImage(Gdiplus::Graphics& g,Gdiplus::Bitmap& source,
    std::unique_ptr<Gdiplus::Bitmap>& cache,const Gdiplus::RectF& bounds,const Gdiplus::RectF& crop)
{
    Gdiplus::Matrix transform;g.GetTransform(&transform);
    Gdiplus::REAL elements[6]{};transform.GetElements(elements);
    const float scaleX=std::hypot(elements[0],elements[1]),scaleY=std::hypot(elements[2],elements[3]);
    const int width=std::clamp(static_cast<int>(std::ceil(bounds.Width*scaleX)),1,8192);
    const int height=std::clamp(static_cast<int>(std::ceil(bounds.Height*scaleY)),1,8192);
    if(!cache || cache->GetWidth()!=static_cast<UINT>(width) || cache->GetHeight()!=static_cast<UINT>(height)) {
        auto resized=std::make_unique<Gdiplus::Bitmap>(width,height,PixelFormat32bppPARGB);
        Gdiplus::Graphics prepare(resized.get());
        prepare.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        prepare.Clear(Gdiplus::Color(0,0,0,0));
        prepare.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        prepare.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        const auto status=prepare.DrawImage(&source,Gdiplus::RectF(0,0,static_cast<float>(width),static_cast<float>(height)),
            crop.X,crop.Y,crop.Width,crop.Height,Gdiplus::UnitPixel);
        prepare.Flush(Gdiplus::FlushIntentionSync);
        if(status==Gdiplus::Ok && resized->GetLastStatus()==Gdiplus::Ok) cache=std::move(resized);
        else {
            g.DrawImage(&source,bounds,crop.X,crop.Y,crop.Width,crop.Height,Gdiplus::UnitPixel);
            return;
        }
    }
    const auto saved=g.Save();
    g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    g.DrawImage(cache.get(),bounds,0,0,static_cast<float>(width),static_cast<float>(height),Gdiplus::UnitPixel);
    g.Restore(saved);
}

void DrawingUtils::drawDisplayText(Gdiplus::Graphics& g, std::wstring_view text,
                                   const Gdiplus::RectF& bounds, float size, const Gdiplus::Color& color)
{
    if (!DisplayFamily) { drawText(g,text,bounds,size,Gdiplus::FontStyleBold,color); return; }
    Gdiplus::Font font(DisplayFamily,size,Gdiplus::FontStyleBold,Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(color);
    Gdiplus::StringFormat format;
    format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    g.DrawString(text.data(),static_cast<INT>(text.size()),&font,bounds,&format,&brush);
}

std::unique_ptr<Gdiplus::GraphicsPath> DrawingUtils::createRoundedPath(
    const Gdiplus::RectF& bounds, float radius)
{
    auto path = std::make_unique<Gdiplus::GraphicsPath>();
    if (bounds.Width <= 0.0F || bounds.Height <= 0.0F) return path;
    const float diameter = std::min(radius * 2.0F, std::min(bounds.Width, bounds.Height));
    const Gdiplus::RectF arc(bounds.X, bounds.Y, diameter, diameter);

    path->AddArc(arc, 180.0F, 90.0F);
    path->AddArc(bounds.GetRight() - diameter, bounds.Y, diameter, diameter, 270.0F, 90.0F);
    path->AddArc(bounds.GetRight() - diameter, bounds.GetBottom() - diameter,
                 diameter, diameter, 0.0F, 90.0F);
    path->AddArc(bounds.X, bounds.GetBottom() - diameter, diameter, diameter, 90.0F, 90.0F);
    path->CloseFigure();
    return path;
}

void DrawingUtils::fillRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                                        float radius, const Gdiplus::Color& color)
{
    auto path = createRoundedPath(bounds, radius);
    Gdiplus::SolidBrush brush(color);
    graphics.FillPath(&brush, path.get());
}

void DrawingUtils::drawRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                                        float radius, const Gdiplus::Color& color, float width)
{
    auto path = createRoundedPath(bounds, radius);
    Gdiplus::Pen pen(color, width);
    graphics.DrawPath(&pen, path.get());
}

void DrawingUtils::drawText(Gdiplus::Graphics& graphics, std::wstring_view text,
                            const Gdiplus::RectF& bounds, float size, int style,
                            const Gdiplus::Color& color,
                            Gdiplus::StringAlignment horizontal,
                            Gdiplus::StringAlignment vertical)
{
    Gdiplus::Font font(Theme::FontFamily, size, style, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(color);
    Gdiplus::StringFormat format;
    format.SetAlignment(horizontal);
    format.SetLineAlignment(vertical);
    format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    graphics.DrawString(text.data(), static_cast<INT>(text.size()), &font, bounds, &format, &brush);
}

Gdiplus::Color DrawingUtils::colorForTone(StatusTone tone)
{
    switch (tone)
    {
    case StatusTone::Success:
        return Theme::Success;
    case StatusTone::Information:
        return Theme::Information;
    case StatusTone::Warning:
        return Theme::Warning;
    case StatusTone::Error:
        return Theme::Error;
    default:
        return Theme::TextMuted;
    }
}

bool DrawingUtils::contains(const Gdiplus::RectF& bounds, float x, float y)
{
    return x >= bounds.X && x <= bounds.GetRight() &&
           y >= bounds.Y && y <= bounds.GetBottom();
}

Gdiplus::Color DrawingUtils::blend(const Gdiplus::Color& from,const Gdiplus::Color& to,float amount)
{
    amount=std::clamp(amount,0.0F,1.0F);
    const auto channel=[amount](BYTE a,BYTE b) {
        return static_cast<BYTE>(static_cast<float>(a)+(static_cast<float>(b)-a)*amount);
    };
    return {channel(from.GetA(),to.GetA()),channel(from.GetR(),to.GetR()),
            channel(from.GetG(),to.GetG()),channel(from.GetB(),to.GetB())};
}
void DrawingUtils::drawLogo(Gdiplus::Graphics& g,const Gdiplus::RectF& b,const Gdiplus::Color& color)
{
    const auto saved=g.Save();
    g.TranslateTransform(b.X,b.Y); g.ScaleTransform(b.Width/48,b.Height/48);
    const Gdiplus::PointF points[]={{8,4},{35,4},{43,12},{19,12},{13,18},{34,18},
        {34,26},{13,26},{19,36},{43,36},{35,44},{8,44},{3,35},{3,13}};
    Gdiplus::SolidBrush brush(color); g.FillPolygon(&brush,points,14);
    const Gdiplus::PointF gate[]={{39,17},{46,24},{39,31},{35,27},{38,24},{35,21}};
    g.FillPolygon(&brush,gate,6);
    g.Restore(saved);
}
