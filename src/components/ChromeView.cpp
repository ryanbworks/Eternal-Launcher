#include "components/ChromeView.h"

#include "components/Layout.h"
#include "config/LauncherConfig.h"
#include "constants/LauncherConstants.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

namespace
{
    std::wstring versionPill()
    {
        // "2.0.0-Horizon" becomes "Horizon · 2.0.0".
        const std::wstring version(LauncherConfig::LauncherVersion);
        const auto dash = version.find(L'-');
        if (dash == std::wstring::npos) return version;
        return version.substr(dash + 1) + L" · " + version.substr(0, dash);
    }
}

void ChromeView::draw(Gdiplus::Graphics& g, float width, float height, LauncherPage page, bool maximized,
                      bool active, const ControlVisuals& visuals, float indicatorY, int newsBadge, float time) const
{
    using namespace LauncherConstants;
    Gdiplus::SolidBrush background(Theme::Background);
    g.FillRectangle(&background, 0.0F, 0.0F, width, TitleBarHeight);
    g.FillRectangle(&background, 0.0F, TitleBarHeight, SidebarWidth, height - TitleBarHeight);
    Gdiplus::Pen divider(Theme::Divider);
    g.DrawLine(&divider, 0.0F, TitleBarHeight - .5F, width, TitleBarHeight - .5F);
    g.DrawLine(&divider, SidebarWidth - .5F, TitleBarHeight, SidebarWidth - .5F, height);

    emblem_.draw(g, {15, 8, 28, 28});
    DrawingUtils::drawDisplayText(g, L"eternal", {48, 10, 90, 24}, 15, active ? Theme::TextPrimary : Theme::TextSecondary,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    const auto pill = versionPill();
    const float pillWidth = DrawingUtils::measureText(g, pill, 11, Gdiplus::FontStyleBold) + 20;
    const Gdiplus::RectF pillBounds{120, 12, pillWidth, 20};
    DrawingUtils::fillPill(g, pillBounds, Theme::AccentTint);
    DrawingUtils::drawRoundedRectangle(g, {pillBounds.X + .5F, pillBounds.Y + .5F, pillBounds.Width - 1, pillBounds.Height - 1}, 10,
        Gdiplus::Color(255, 47, 40, 80));
    DrawingUtils::drawText(g, pill, pillBounds, 11, Gdiplus::FontStyleBold, Theme::AccentLight,
        Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);

    const Gdiplus::Color closeHover(255, 196, 43, 28);
    const Gdiplus::Color captionHover(255, 30, 31, 44);
    const struct { int id; Icons::Icon icon; } caption[] = {
        {MinimizeButtonId, Icons::Icon::Minimize},
        {MaximizeButtonId, maximized ? Icons::Icon::Restore : Icons::Icon::Maximize},
        {CloseButtonId, Icons::Icon::Close}};
    for (int i = 0; i < 3; ++i) {
        const auto bounds = Layout::captionButton(i, width);
        const auto visual = visuals(caption[i].id);
        const auto& fillColor = caption[i].id == CloseButtonId ? closeHover : captionHover;
        const float amount = visual.pressed ? 1.0F : visual.hover;
        if (amount > .01F) {
            Gdiplus::SolidBrush hover(DrawingUtils::blend(Gdiplus::Color(0, fillColor.GetR(), fillColor.GetG(), fillColor.GetB()), fillColor, amount));
            g.FillRectangle(&hover, bounds);
        }
        const auto ink = caption[i].id == CloseButtonId && amount > .5F ? Theme::TextPrimary :
            DrawingUtils::blend(Theme::TextSecondary, Theme::TextPrimary, amount);
        Icons::draw(g, caption[i].icon, {bounds.X + 17, bounds.Y + 16, 12, 12}, ink, 1.2F);
        if (visual.focused) DrawingUtils::drawRoundedRectangle(g, {bounds.X + 3, bounds.Y + 3, bounds.Width - 6, bounds.Height - 6}, 4, Theme::AccentLight, 2);
    }

    const struct { int id; Icons::Icon icon; LauncherPage page; } navigation[] = {
        {NavHomeId, Icons::Icon::Home, LauncherPage::Home},
        {NavVersionsId, Icons::Icon::Layers, LauncherPage::Versions},
        {NavNewsId, Icons::Icon::News, LauncherPage::News},
        {NavModsId, Icons::Icon::Mods, LauncherPage::Mods},
        {NavScreenshotsId, Icons::Icon::Image, LauncherPage::Screenshots},
        {NavSettingsId, Icons::Icon::Settings, LauncherPage::Settings}};
    // One selection pill travels between the icons, with an accent bar on the rail edge.
    const Gdiplus::RectF indicator{14, indicatorY, 48, 48};
    DrawingUtils::fillRoundedRectangle(g, indicator, 14, Theme::AccentTint);
    DrawingUtils::fillPill(g, {2, indicatorY + 12, 4, 24}, Theme::Accent);
    const auto icon = [&](const Gdiplus::RectF& bounds, Icons::Icon glyph, int id, bool selected) {
        auto visual = visuals(id);
        if (selected) visual.hover = 0;  // the pill already marks it
        Buttons::drawIcon(g, bounds, glyph, visual, false, 22);
        if (selected) Icons::draw(g, glyph, {bounds.X + 13, bounds.Y + 13, 22, 22}, Theme::AccentLight, 1.8F);
    };
    for (int i = 0; i < static_cast<int>(std::size(navigation)); ++i)
        icon(Layout::navButton(i), navigation[i].icon, navigation[i].id, page == navigation[i].page);
    icon(Layout::sidebarHelp(height), Icons::Icon::Help, NavHelpId, page == LauncherPage::Help);

    if (newsBadge > 0) {
        // Unread news: a counter on the corner of the News icon, with a soft ring that keeps
        // calling while it is there.
        const auto news = Layout::navButton(static_cast<int>(LauncherPage::News));
        const std::wstring count = newsBadge > 9 ? std::wstring(L"9+") : std::to_wstring(newsBadge);
        const float badgeWidth = std::max(18.0F, DrawingUtils::measureText(g, count, 10.5F, Gdiplus::FontStyleBold) + 9);
        const Gdiplus::RectF badge{news.GetRight() - badgeWidth + 4, news.Y + 3, badgeWidth, 18};
        if (time >= 0) {
            const float t = std::fmod(time, 2.2F) / 2.2F;
            const float grow = 7 * (1 - (1 - t) * (1 - t));
            DrawingUtils::drawRoundedRectangle(g, {badge.X - grow, badge.Y - grow, badge.Width + 2 * grow, badge.Height + 2 * grow},
                9 + grow, Gdiplus::Color(static_cast<BYTE>(150 * (1 - t)), 165, 130, 255), 1.5F);
        }
        DrawingUtils::fillPill(g, {badge.X - 2, badge.Y - 2, badge.Width + 4, badge.Height + 4}, Theme::Background);
        DrawingUtils::fillPill(g, badge, Theme::Accent);
        DrawingUtils::drawText(g, count, badge, 10.5F, Gdiplus::FontStyleBold, Theme::AccentInk,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
    }
}
