#pragma once

#include <objidl.h>
#include <gdiplus.h>
#include <memory>

class HeaderComponent
{
public:
    HeaderComponent();
    ~HeaderComponent();
    HeaderComponent(const HeaderComponent&) = delete;
    HeaderComponent& operator=(const HeaderComponent&) = delete;

    void draw(Gdiplus::Graphics& graphics, float width, float hover = 0, bool focused = false, bool showSettings = true) const;
    [[nodiscard]] Gdiplus::RectF settingsBounds(float width) const;

private:
    void loadLogo();

    std::unique_ptr<Gdiplus::Bitmap> logo_;
    mutable std::unique_ptr<Gdiplus::Bitmap> scaledLogo_;
    IStream* logoStream_ = nullptr;
};
