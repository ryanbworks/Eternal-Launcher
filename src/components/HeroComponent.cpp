#include "components/HeroComponent.h"

#include "config/LauncherConfig.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <array>
#include <string>

namespace
{
    void drawChip(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                  const std::wstring& label)
    {
        DrawingUtils::fillRoundedRectangle(graphics, bounds, 16.0F,
                                           Gdiplus::Color(120, 32, 35, 48));
        DrawingUtils::drawRoundedRectangle(graphics, bounds, 16.0F,
                                           Gdiplus::Color(170, 71, 65, 101));
        DrawingUtils::drawText(graphics, label, bounds, 11.0F, Gdiplus::FontStyleBold,
                               Theme::TextPrimary, Gdiplus::StringAlignmentCenter,
                               Gdiplus::StringAlignmentCenter);
    }
}

void HeroComponent::draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds) const
{
    auto cardPath = DrawingUtils::createRoundedPath(bounds, 24.0F);
    Gdiplus::LinearGradientBrush background(
        bounds, Gdiplus::Color(255, 32, 27, 55), Gdiplus::Color(255, 17, 22, 34),
        Gdiplus::LinearGradientModeHorizontal);
    graphics.FillPath(&background, cardPath.get());
    DrawingUtils::drawRoundedRectangle(graphics, bounds, 24.0F,
                                       Gdiplus::Color(255, 63, 53, 91));

    // Formas suaves criam profundidade sem competir com as informações principais.
    Gdiplus::SolidBrush glowBrush(Gdiplus::Color(38, 167, 139, 250));
    graphics.FillEllipse(&glowBrush, bounds.GetRight() - 205.0F, bounds.Y - 55.0F, 275.0F, 275.0F);
    Gdiplus::Pen orbitPen(Gdiplus::Color(70, 167, 139, 250), 1.2F);
    graphics.DrawEllipse(&orbitPen, bounds.GetRight() - 180.0F, bounds.Y + 20.0F, 130.0F, 130.0F);
    graphics.DrawEllipse(&orbitPen, bounds.GetRight() - 145.0F, bounds.Y + 55.0F, 60.0F, 60.0F);

    DrawingUtils::drawText(graphics, L"ETERNAL REALMS  •  TEMPORADA I",
                           {bounds.X + 28.0F, bounds.Y + 28.0F, 390.0F, 22.0F},
                           11.0F, Gdiplus::FontStyleBold, Theme::AccentLight);
    DrawingUtils::drawText(graphics, L"Sua próxima aventura\ncomeça aqui.",
                           {bounds.X + 28.0F, bounds.Y + 62.0F, 470.0F, 92.0F},
                           34.0F, Gdiplus::FontStyleBold, Theme::TextPrimary);
    DrawingUtils::drawText(graphics,
                           L"Uma instalação privada, sincronizada e preparada para o nosso grupo.",
                           {bounds.X + 30.0F, bounds.Y + 160.0F, 500.0F, 28.0F},
                           14.0F, Gdiplus::FontStyleRegular, Theme::TextSecondary);

    const std::array<std::wstring, 2> chips = {
        L"MINECRAFT  " + std::wstring(LauncherConfig::MinecraftVersion),
        L"FORGE  " + std::wstring(LauncherConfig::ForgeVersion)
    };
    const std::array<float, 3> widths = {154.0F, 138.0F, 90.0F};
    float chipX = bounds.X + 28.0F;
    for (std::size_t index = 0; index < chips.size(); ++index)
    {
        drawChip(graphics, {chipX, bounds.Y + 207.0F, widths[index], 32.0F}, chips[index]);
        chipX += widths[index] + 10.0F;
    }

    // Símbolo abstrato do mundo: uma runa própria evita copiar identidades existentes.
    const Gdiplus::PointF runePoints[] = {
        {bounds.GetRight() - 128.0F, bounds.Y + 70.0F},
        {bounds.GetRight() - 76.0F, bounds.Y + 130.0F},
        {bounds.GetRight() - 128.0F, bounds.Y + 191.0F},
        {bounds.GetRight() - 180.0F, bounds.Y + 130.0F},
    };
    Gdiplus::LinearGradientBrush runeBrush(
        Gdiplus::RectF(bounds.GetRight() - 180.0F, bounds.Y + 70.0F, 104.0F, 121.0F),
        Theme::AccentLight, Theme::Accent, Gdiplus::LinearGradientModeVertical);
    graphics.FillPolygon(&runeBrush, runePoints, 4);
    Gdiplus::SolidBrush cutout(Gdiplus::Color(255, 29, 25, 48));
    graphics.FillEllipse(&cutout, bounds.GetRight() - 147.0F, bounds.Y + 108.0F, 38.0F, 44.0F);
}
