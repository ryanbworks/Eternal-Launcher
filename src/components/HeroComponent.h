#pragma once

#include <objidl.h>
#include <gdiplus.h>
#include <memory>

class HeroComponent
{
public:
    HeroComponent();
    ~HeroComponent();
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, float reveal = 1,
              float energy = 0, float phase = 0, float drift = 0) const;
private:
    IStream* stream_ = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> artwork_;
    mutable std::unique_ptr<Gdiplus::Bitmap> scaledArtwork_;
};
