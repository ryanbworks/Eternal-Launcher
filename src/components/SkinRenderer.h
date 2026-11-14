#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The pixels of a Minecraft skin texture (64 x 64, or the legacy 64 x 32), ARGB.
struct SkinTexture
{
    int width = 64;
    int height = 64;
    std::vector<std::uint32_t> pixels;
    [[nodiscard]] std::uint32_t at(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < width && y < height ? pixels[static_cast<std::size_t>(y * width + x)] : 0;
    }
};

namespace SkinRenderer
{
    // Decodes a skin PNG (GDI+ must be running). Only 64 x 64 and 64 x 32 are accepted.
    std::optional<SkinTexture> decode(const std::string& png);
    // Slim (3-pixel) arms leave the 4th arm column empty in 64 x 64 skins.
    bool looksSlim(const SkinTexture& skin);
    // Draws the whole body, flat, front or back, filling `bounds` (16 x 32 skin pixels),
    // with the outer layer (hat, jacket, sleeves, pants) on top.
    void drawBody(Gdiplus::Graphics& graphics, const SkinTexture& skin, const Gdiplus::RectF& bounds, bool slim, bool back);
    // The whole figure in 3D, turned `angle` radians around its vertical axis (0 faces the
    // viewer), seen level and without perspective. Fills `bounds` like drawBody.
    void drawTurned(Gdiplus::Graphics& graphics, const SkinTexture& skin, const Gdiplus::RectF& bounds, bool slim, float angle);
}
