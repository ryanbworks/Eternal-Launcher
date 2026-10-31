#include "utils/BrandEmblem.h"

#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <windows.h>

#include <cstring>

BrandEmblem::~BrandEmblem()
{
    scaled_.reset();
    image_.reset();
    if (stream_) stream_->Release();
}

void BrandEmblem::load() const
{
    // Loaded on first use, after GDI+ is running.
    loaded_ = true;
    const auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(105), RT_RCDATA);
    if (!resource) return;
    const auto loadedResource = LoadResource(nullptr, resource);
    const auto data = loadedResource ? LockResource(loadedResource) : nullptr;
    const auto size = loadedResource ? SizeofResource(nullptr, resource) : 0;
    if (!data || !size) return;
    const auto memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return;
    const auto target = GlobalLock(memory);
    if (!target) { GlobalFree(memory); return; }
    std::memcpy(target, data, size);
    GlobalUnlock(memory);
    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) { GlobalFree(memory); return; }
    std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromStream(stream));
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) { bitmap.reset(); stream->Release(); return; }
    stream_ = stream;
    image_ = std::move(bitmap);
}

void BrandEmblem::draw(Gdiplus::Graphics& g, const Gdiplus::RectF& b) const
{
    if (!loaded_) load();
    if (image_) {
        DrawingUtils::drawCachedImage(g, *image_, scaled_, b,
            {0, 0, static_cast<float>(image_->GetWidth()), static_cast<float>(image_->GetHeight())});
        return;
    }
    // Without the resource (unit tests), keep a neutral brand-colored mark in place.
    DrawingUtils::fillRoundedRectangle(g, {b.X + b.Width * .15F, b.Y + b.Height * .15F, b.Width * .7F, b.Height * .7F},
        b.Width * .2F, Theme::Accent);
}
