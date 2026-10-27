#pragma once

#include <objidl.h>
#include <gdiplus.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

class HeroComponent
{
public:
    HeroComponent();
    ~HeroComponent();
    void draw(Gdiplus::Graphics& graphics,const Gdiplus::RectF& bounds,float reveal,float energy,float phase,float drift,
              const std::vector<std::wstring>& chips) const;
    // Crop of the landscape used as a thumbnail; focus is the normalized point kept in view.
    void drawThumbnail(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, std::size_t slot,
                       float focusX, float focusY) const;
private:
    void drawStatic(Gdiplus::Graphics& graphics,const Gdiplus::RectF& bounds,float reveal,const std::vector<std::wstring>& chips) const;
    bool drawStaticCached(Gdiplus::Graphics& graphics,const Gdiplus::RectF& bounds,const std::vector<std::wstring>& chips) const;
    IStream* stream_ = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> artwork_;
    mutable std::unique_ptr<Gdiplus::Bitmap> scaledArtwork_;
    mutable std::array<std::unique_ptr<Gdiplus::Bitmap>, 4> thumbnails_;
    // Landscape, shading, glass panel and copy at the current device size.
    mutable std::unique_ptr<Gdiplus::Bitmap> staticLayer_;
    mutable std::wstring staticKey_;
};
