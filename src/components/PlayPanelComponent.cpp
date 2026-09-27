#include "components/PlayPanelComponent.h"

#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

void PlayPanelComponent::draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
                              const LauncherViewState& state) const
{
    auto cardPath = DrawingUtils::createRoundedPath(bounds, 18.0F);
    Gdiplus::LinearGradientBrush cardBrush(
        bounds, Gdiplus::Color(255, 26, 30, 41), Gdiplus::Color(255, 21, 23, 32),
        Gdiplus::LinearGradientModeVertical);
    graphics.FillPath(&cardBrush, cardPath.get());
    DrawingUtils::drawRoundedRectangle(graphics, bounds, 18.0F, Theme::Border);

    const auto stateColor = state.playEnabled ? Theme::Success : Theme::AccentLight;
    Gdiplus::SolidBrush dotBrush(stateColor);
    graphics.FillEllipse(&dotBrush, bounds.X + 24.0F, bounds.Y + 22.0F, 8.0F, 8.0F);
    DrawingUtils::drawText(graphics, L"STATUS ATUAL",
                           {bounds.X + 42.0F, bounds.Y + 16.0F, 180.0F, 20.0F},
                           10.0F, Gdiplus::FontStyleBold, Theme::TextMuted);
    DrawingUtils::drawText(graphics, state.headline,
                           {bounds.X + 24.0F, bounds.Y + 47.0F, bounds.Width - 48.0F, 30.0F},
                           20.0F, Gdiplus::FontStyleBold, Theme::TextPrimary);
    DrawingUtils::drawText(graphics, state.operationMessage,
                           {bounds.X + 24.0F, bounds.Y + 79.0F, bounds.Width - 48.0F, 34.0F},
                           11.0F, Gdiplus::FontStyleRegular, Theme::TextSecondary);

    const auto button = buttonBounds(bounds);
    auto buttonPath = DrawingUtils::createRoundedPath(button, 14.0F);
    const auto startColor = state.playEnabled ? Theme::AccentLight : Theme::TextMuted;
    const auto endColor = state.playEnabled ? Theme::Accent : Theme::SurfaceHover;
    Gdiplus::LinearGradientBrush buttonBrush(
        button, startColor, endColor, Gdiplus::LinearGradientModeHorizontal);
    graphics.FillPath(&buttonBrush, buttonPath.get());

    const std::wstring_view buttonLabel = state.playEnabled
        ? L"JOGAR AGORA"
        : state.operation == LauncherOperation::ServerUnavailable
            ? L"SERVIDOR OFFLINE"
            : L"AGUARDE...";
    DrawingUtils::drawText(graphics, buttonLabel, button, 15.0F, Gdiplus::FontStyleBold,
                           Theme::TextPrimary, Gdiplus::StringAlignmentCenter,
                           Gdiplus::StringAlignmentCenter);

    if (state.playEnabled)
    {
        // Triângulo simples reforça a ação primária sem depender de uma biblioteca de ícones.
        const float iconX = button.X + 90.0F;
        const float iconY = button.Y + button.Height / 2.0F;
        const Gdiplus::PointF playIcon[] = {
            {iconX - 5.0F, iconY - 7.0F},
            {iconX + 7.0F, iconY},
            {iconX - 5.0F, iconY + 7.0F}
        };
        Gdiplus::SolidBrush iconBrush(Theme::TextPrimary);
        graphics.FillPolygon(&iconBrush, playIcon, 3);
    }
}

Gdiplus::RectF PlayPanelComponent::buttonBounds(const Gdiplus::RectF& bounds) const
{
    return {bounds.X + 24.0F, bounds.Y + 127.0F, bounds.Width - 48.0F, 60.0F};
}
