#include "components/SkinRenderer.h"

#include "services/GalleryApi.h"

#include <algorithm>

namespace
{
    // One face of a body part: its rectangle on the texture and where it lands on the
    // 16 x 32 flat figure. `mirror` flips it (legacy skins reuse the right limbs).
    struct Face { int u, v, w, h, x, y; bool mirror = false; };

    void blit(Gdiplus::Graphics& g, const SkinTexture& skin, const Face& face, float originX, float originY, float cell, bool overlay)
    {
        for (int row = 0; row < face.h; ++row)
            for (int column = 0; column < face.w; ++column) {
                const int sourceX = face.u + (face.mirror ? face.w - 1 - column : column);
                const std::uint32_t argb = skin.at(sourceX, face.v + row);
                const auto alpha = static_cast<BYTE>(argb >> 24);
                // The base layer is opaque in the game; the outer layer keeps its transparency.
                if (alpha == 0 || (overlay && alpha < 16)) continue;
                Gdiplus::SolidBrush brush(Gdiplus::Color(overlay ? alpha : 255, static_cast<BYTE>(argb >> 16), static_cast<BYTE>(argb >> 8),
                    static_cast<BYTE>(argb)));
                g.FillRectangle(&brush, originX + static_cast<float>(face.x + column) * cell, originY + static_cast<float>(face.y + row) * cell,
                    cell + .5F, cell + .5F);
            }
    }
}

std::optional<SkinTexture> SkinRenderer::decode(const std::string& png)
{
    auto bitmap = GalleryApi::decode(png);
    if (!bitmap) return std::nullopt;
    const int width = static_cast<int>(bitmap->GetWidth()), height = static_cast<int>(bitmap->GetHeight());
    if (width != 64 || (height != 64 && height != 32)) return std::nullopt;
    SkinTexture skin;
    skin.width = width;
    skin.height = height;
    skin.pixels.resize(static_cast<std::size_t>(width * height));
    Gdiplus::Rect area(0, 0, width, height);
    Gdiplus::BitmapData data{};
    if (bitmap->LockBits(&area, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok) return std::nullopt;
    for (int y = 0; y < height; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const BYTE*>(data.Scan0) + y * data.Stride);
        std::copy(row, row + width, skin.pixels.begin() + static_cast<std::ptrdiff_t>(y * width));
    }
    bitmap->UnlockBits(&data);
    return skin;
}

bool SkinRenderer::looksSlim(const SkinTexture& skin)
{
    if (skin.height != 64) return false;
    // Column 54-55 of the right arm's front/top area is unused by 3-pixel arms.
    for (int y = 20; y < 32; ++y)
        if ((skin.at(54, y) >> 24) != 0 || (skin.at(55, y) >> 24) != 0) return false;
    return true;
}

void SkinRenderer::drawBody(Gdiplus::Graphics& g, const SkinTexture& skin, const Gdiplus::RectF& bounds, bool slim, bool back)
{
    const float cell = std::min(bounds.Width / 16, bounds.Height / 32);
    const float originX = bounds.X + (bounds.Width - 16 * cell) / 2, originY = bounds.Y + (bounds.Height - 32 * cell) / 2;
    const bool legacy = skin.height == 32;
    const int arm = slim ? 3 : 4;
    const auto saved = g.Save();
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    // Seen from the front, the character's right side is on the viewer's left.
    std::vector<Face> base, outer;
    if (!back) {
        base = {
            {8, 8, 8, 8, 4, 0},                                      // head
            {20, 20, 8, 12, 4, 8},                                   // body
            {44, 20, arm, 12, 4 - arm, 8},                           // right arm
            legacy ? Face{44, 20, arm, 12, 12, 8, true} : Face{36, 52, arm, 12, 12, 8},  // left arm
            {4, 20, 4, 12, 4, 20},                                   // right leg
            legacy ? Face{4, 20, 4, 12, 8, 20, true} : Face{20, 52, 4, 12, 8, 20}};      // left leg
        outer = {{40, 8, 8, 8, 4, 0}};
        if (!legacy) outer.insert(outer.end(), {{20, 36, 8, 12, 4, 8}, {44, 36, arm, 12, 4 - arm, 8}, {52, 52, arm, 12, 12, 8},
                                                {4, 36, 4, 12, 4, 20}, {4, 52, 4, 12, 8, 20}});
    } else {
        // From behind the left limbs are on the viewer's left.
        const int armBack = slim ? 51 : 52, leftArmBack = slim ? 43 : 44;
        base = {
            {24, 8, 8, 8, 4, 0},
            {32, 20, 8, 12, 4, 8},
            {armBack, 20, arm, 12, 12, 8},
            legacy ? Face{52, 20, arm, 12, 4 - arm, 8, true} : Face{leftArmBack, 52, arm, 12, 4 - arm, 8},
            {12, 20, 4, 12, 8, 20},
            legacy ? Face{12, 20, 4, 12, 4, 20, true} : Face{28, 52, 4, 12, 4, 20}};
        outer = {{56, 8, 8, 8, 4, 0}};
        if (!legacy) outer.insert(outer.end(), {{32, 36, 8, 12, 4, 8}, {armBack, 36, arm, 12, 12, 8}, {leftArmBack + 16, 52, arm, 12, 4 - arm, 8},
                                                {12, 36, 4, 12, 8, 20}, {12, 52, 4, 12, 4, 20}});
    }
    for (const auto& face : base) blit(g, skin, face, originX, originY, cell, false);
    for (const auto& face : outer) blit(g, skin, face, originX, originY, cell, true);
    g.Restore(saved);
}
