#pragma once

#include <objidl.h>
#include <gdiplus.h>

#include <memory>

// The Eternal emblem (the hexagon with the star, also used by the application icon),
// embedded as RCDATA 105 and drawn from the same artwork everywhere in the window.
class BrandEmblem
{
public:
    BrandEmblem() = default;
    ~BrandEmblem();
    BrandEmblem(const BrandEmblem&) = delete;
    BrandEmblem& operator=(const BrandEmblem&) = delete;
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds) const;
private:
    void load() const;
    mutable bool loaded_ = false;
    mutable IStream* stream_ = nullptr;
    mutable std::unique_ptr<Gdiplus::Bitmap> image_;
    mutable std::unique_ptr<Gdiplus::Bitmap> scaled_;
};
