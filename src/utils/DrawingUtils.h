#pragma once

#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

#include <windows.h>
#include <cstdint>
#include <string_view>
#include <memory>
#include <unordered_map>

namespace DrawingUtils
{
    class DisplayTypeface
    {
    public:
        DisplayTypeface();
        ~DisplayTypeface();
    private:
        Gdiplus::PrivateFontCollection collection_;
        Gdiplus::FontFamily family_;
        // Also registered with GDI so native EDIT controls can use the display face.
        HANDLE gdiFont_ = nullptr;
        // Fonts by face/size/style. Looking a family up by name on every DrawString was a
        // large share of each frame; the cache lives (and dies) with GDI+ itself.
        std::unordered_map<std::uint64_t, std::unique_ptr<Gdiplus::Font>> fonts_;
        friend const Gdiplus::Font* cachedFont(bool display, float size, int style, std::unique_ptr<Gdiplus::Font>& owned);
    };
    // Returns a cached font while a DisplayTypeface exists; otherwise fills `owned`.
    const Gdiplus::Font* cachedFont(bool display, float size, int style, std::unique_ptr<Gdiplus::Font>& owned);
    // Family name for CreateFont when the embedded display face is available.
    const wchar_t* displayFontFamily();
    void drawDisplayText(Gdiplus::Graphics& graphics, std::wstring_view text,
                         const Gdiplus::RectF& bounds, float size, const Gdiplus::Color& color,
                         Gdiplus::StringAlignment horizontal = Gdiplus::StringAlignmentNear,
                         Gdiplus::StringAlignment vertical = Gdiplus::StringAlignmentNear);
    float measureText(Gdiplus::Graphics& graphics, std::wstring_view text, float size, int style);
    void drawCachedImage(Gdiplus::Graphics& graphics, Gdiplus::Bitmap& source,
                         std::unique_ptr<Gdiplus::Bitmap>& cache, const Gdiplus::RectF& bounds,
                         const Gdiplus::RectF& crop);
    std::unique_ptr<Gdiplus::GraphicsPath> createRoundedPath(
        const Gdiplus::RectF& bounds, float radius);
    std::unique_ptr<Gdiplus::GraphicsPath> createRoundedPath(
        const Gdiplus::RectF& bounds, float topLeft, float topRight, float bottomRight, float bottomLeft);
    void fillRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                              float topLeft, float topRight, float bottomRight, float bottomLeft,
                              const Gdiplus::Color& color);
    void drawFocusRing(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, float radius,
                       const Gdiplus::Color& color);
    void fillPill(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds, const Gdiplus::Color& color);
    void fillRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                              float radius, const Gdiplus::Color& color);
    void drawRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                              float radius, const Gdiplus::Color& color, float width = 1.0F);
    void drawText(Gdiplus::Graphics& graphics, std::wstring_view text,
                  const Gdiplus::RectF& bounds, float size, int style,
                  const Gdiplus::Color& color,
                  Gdiplus::StringAlignment horizontal = Gdiplus::StringAlignmentNear,
                  Gdiplus::StringAlignment vertical = Gdiplus::StringAlignmentNear);
    Gdiplus::Color colorForTone(StatusTone tone);
    bool contains(const Gdiplus::RectF& bounds, float x, float y);
    Gdiplus::Color blend(const Gdiplus::Color& from, const Gdiplus::Color& to, float amount);
}
