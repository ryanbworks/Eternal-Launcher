#include "components/HomeView.h"

#include "components/HeroComponent.h"
#include "components/Layout.h"
#include "constants/LauncherConstants.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

using namespace DrawingUtils;

namespace
{
    Gdiplus::Color statusColor(const LauncherViewState& state, bool playing)
    {
        if (playing) return Theme::Success;
        if (isLauncherError(state.operation)) return Theme::Error;
        if (state.operation == LauncherOperation::OfflineReady) return Theme::Warning;
        if (state.operation == LauncherOperation::NeedsInstallation) return Theme::Warning;
        return state.playEnabled ? Theme::Success : Theme::Accent;
    }

    void dot(Gdiplus::Graphics& g, float x, float y, const Gdiplus::Color& color, float size = 8)
    {
        Gdiplus::SolidBrush brush(color);
        g.FillEllipse(&brush, x, y, size, size);
    }

    float easeOut(float t) { t = std::clamp(t, 0.0F, 1.0F); return 1 - (1 - t) * (1 - t) * (1 - t); }
    float easeInOut(float t) { t = std::clamp(t, 0.0F, 1.0F); return t < .5F ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.0F) / 2; }

    // Expanding ring that fades out around a status dot: "something is happening here".
    void pulseRing(Gdiplus::Graphics& g, float cx, float cy, const Gdiplus::Color& color, float time, float period = 1.8F)
    {
        const float t = std::fmod(time, period) / period;
        const float radius = 4 + 9 * easeOut(t);
        Gdiplus::Pen pen(Gdiplus::Color(static_cast<BYTE>(130 * (1 - t)), color.GetR(), color.GetG(), color.GetB()), 1.5F);
        g.DrawEllipse(&pen, cx - radius, cy - radius, 2 * radius, 2 * radius);
    }

    // Vertical band of light, transparent at the edges, used for sweeps and sheens.
    void lightBand(Gdiplus::Graphics& g, float centerX, float top, float height, float width, BYTE alpha)
    {
        if (alpha == 0) return;
        Gdiplus::LinearGradientBrush brush(Gdiplus::PointF(centerX - width / 2, 0), Gdiplus::PointF(centerX + width / 2, 0),
            Gdiplus::Color(0, 255, 255, 255), Gdiplus::Color(0, 255, 255, 255));
        const Gdiplus::Color colors[] = {Gdiplus::Color(0, 255, 255, 255), Gdiplus::Color(alpha, 255, 255, 255), Gdiplus::Color(0, 255, 255, 255)};
        const Gdiplus::REAL positions[] = {0, .5F, 1};
        brush.SetInterpolationColors(colors, positions, 3);
        g.FillRectangle(&brush, centerX - width / 2, top, width, height);
    }
}

void HomeView::drawAvatar(Gdiplus::Graphics& g, const Gdiplus::RectF& b, const Gdiplus::Color& status, const SkinFace& skin,
                          float shimmer)
{
    // The skin's 8 x 8 face, scaled with hard pixel edges inside a rounded frame.
    fillRoundedRectangle(g, b, 18, Gdiplus::Color(255, 47, 40, 80));
    const Gdiplus::RectF face{b.X + 3, b.Y + 3, b.Width - 6, b.Height - 6};
    const auto saved = g.Save();
    auto clip = createRoundedPath(face, 15);
    g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    const float cell = face.Width / 8;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            Gdiplus::SolidBrush brush(Gdiplus::Color(skin.pixels[static_cast<std::size_t>(y * 8 + x)]));
            // Cells overlap by half a pixel so no seams show at fractional scales.
            g.FillRectangle(&brush, face.X + static_cast<float>(x) * cell, face.Y + static_cast<float>(y) * cell, cell + .5F, cell + .5F);
        }
    if (shimmer >= 0) {
        // Looking up the nickname: a soft band of light crosses the face.
        const float center = face.X - face.Width * .5F + face.Width * 2 * std::fmod(shimmer, 1.0F);
        lightBand(g, center, face.Y, face.Height, face.Width * .7F, 80);
    }
    g.Restore(saved);
    Gdiplus::SolidBrush ring(Theme::Surface);
    g.FillEllipse(&ring, b.GetRight() - 15, b.GetBottom() - 15, 18.0F, 18.0F);
    Gdiplus::SolidBrush indicator(status);
    g.FillEllipse(&indicator, b.GetRight() - 12, b.GetBottom() - 12, 12.0F, 12.0F);
}

void HomeView::draw(Gdiplus::Graphics& g, const HomeModel& model, const HeroComponent& hero,
                    const ControlVisuals& visuals, bool drawHero) const
{
    if (drawHero) {
        hero.draw(g, Layout::Hero, model.reveal, model.energy, model.phase, model.drift, model.chips);
        if (model.celebration > 0 && model.celebrationReveal > .01F) drawCelebration(g, model, visuals);
        else if (model.spotlight && model.spotlightReveal > .01F) drawSpotlight(g, model, visuals);
    }
    drawTiles(g, model);
    drawNews(g, model, hero, visuals);
    drawRanking(g, model);
    drawPlayerCard(g, model.player, visuals);
}

Gdiplus::Color HomeView::tierLight(int level)
{
    static const Gdiplus::Color Colors[RankTier::Levels] = {
        Gdiplus::Color(255, 206, 208, 220), Gdiplus::Color(255, 238, 241, 248), Gdiplus::Color(255, 255, 226, 120),
        Gdiplus::Color(255, 150, 242, 236), Gdiplus::Color(255, 206, 178, 236)};
    return Colors[std::clamp(level, 1, RankTier::Levels) - 1];
}

Gdiplus::Color HomeView::tierDeep(int level)
{
    static const Gdiplus::Color Colors[RankTier::Levels] = {
        Gdiplus::Color(255, 112, 114, 128), Gdiplus::Color(255, 150, 158, 176), Gdiplus::Color(255, 214, 158, 36),
        Gdiplus::Color(255, 34, 180, 192), Gdiplus::Color(255, 78, 58, 96)};
    return Colors[std::clamp(level, 1, RankTier::Levels) - 1];
}

void HomeView::drawTierGem(Gdiplus::Graphics& g, const Gdiplus::RectF& b, int level)
{
    const auto light = tierLight(level), deep = tierDeep(level);
    const float w = b.Width, h = b.Height;
    const auto at = [&](float x, float y) { return Gdiplus::PointF(b.X + w * x, b.Y + h * y); };
    // Crown and pavilion facets, lit from the top left.
    const Gdiplus::PointF outline[] = {at(.27F, .12F), at(.73F, .12F), at(.95F, .38F), at(.5F, .92F), at(.05F, .38F)};
    Gdiplus::LinearGradientBrush body(at(0, .1F), at(1, .9F), light, deep);
    g.FillPolygon(&body, outline, 5);
    const Gdiplus::PointF table[] = {at(.27F, .12F), at(.73F, .12F), at(.62F, .38F), at(.38F, .38F)};
    Gdiplus::SolidBrush shine(Gdiplus::Color(120, 255, 255, 255));
    g.FillPolygon(&shine, table, 4);
    const Gdiplus::PointF left[] = {at(.05F, .38F), at(.38F, .38F), at(.5F, .92F)};
    Gdiplus::SolidBrush shade(Gdiplus::Color(50, 255, 255, 255));
    g.FillPolygon(&shade, left, 3);
    const Gdiplus::PointF right[] = {at(.62F, .38F), at(.95F, .38F), at(.5F, .92F)};
    Gdiplus::SolidBrush dark(Gdiplus::Color(60, 0, 0, 0));
    g.FillPolygon(&dark, right, 3);
    Gdiplus::Pen edge(Gdiplus::Color(150, light.GetR(), light.GetG(), light.GetB()), std::max(1.0F, w / 40));
    edge.SetLineJoin(Gdiplus::LineJoinRound);
    g.DrawPolygon(&edge, outline, 5);
}

void HomeView::drawCelebration(Gdiplus::Graphics& g, const HomeModel& model, const ControlVisuals& visuals) const
{
    const int level = model.celebration;
    const auto light = tierLight(level), deep = tierDeep(level);
    const auto tint = [](const Gdiplus::Color& c, BYTE alpha) { return Gdiplus::Color(alpha, c.GetR(), c.GetG(), c.GetB()); };
    const float time = model.player.time;
    const bool ambient = model.player.ambient;
    const float reveal = easeOut(model.celebrationReveal);
    const auto saved = g.Save();
    auto heroClip = createRoundedPath(Layout::Hero, Theme::PanelRadius);
    g.SetClip(heroClip.get(), Gdiplus::CombineModeIntersect);

    // The landscape dims behind the moment.
    Gdiplus::SolidBrush veil(Gdiplus::Color(static_cast<BYTE>(170 * reveal), 9, 9, 15));
    g.FillRectangle(&veil, Layout::Hero);
    // Confetti in the level's colors and the launcher's violet keeps falling.
    const Gdiplus::Color confetti[] = {light, deep, Theme::Accent, Theme::AccentLight};
    for (int i = 0; i < 46; ++i) {
        const float seed = static_cast<float>(i);
        const float speed = .16F + .09F * std::fmod(seed * .37F, 1.0F);
        const float life = std::fmod((ambient ? time : 1.2F) * speed + seed * .6180339F, 1.0F);
        const float x = Layout::Hero.X + Layout::Hero.Width * std::fmod(seed * .2357F + .05F, 1.0F) + 14 * std::sin(life * 9 + seed);
        const float y = Layout::Hero.Y - 12 + (Layout::Hero.Height + 24) * life;
        const float spin = (ambient ? time : 1.2F) * (2 + std::fmod(seed, 3.0F)) + seed;
        const float width = 3 + std::fmod(seed * 1.7F, 4.0F), height = 7 * std::abs(std::cos(spin));
        Gdiplus::SolidBrush piece(tint(confetti[i % 4], static_cast<BYTE>(210 * reveal)));
        g.FillRectangle(&piece, x, y, width, std::max(1.2F, height));
    }

    g.TranslateTransform(0, std::round(24 * (1 - reveal)));
    const auto& c = Layout::Celebration;
    // A halo in the level's color around the card.
    for (int layer = 5; layer >= 1; --layer) {
        const float grow = static_cast<float>(layer) * 4;
        fillRoundedRectangle(g, {c.X - grow, c.Y - grow, c.Width + 2 * grow, c.Height + 2 * grow}, 20 + grow,
            tint(light, static_cast<BYTE>(14 * reveal / static_cast<float>(layer) * 2)));
    }
    fillRoundedRectangle(g, c, 20, tint(Gdiplus::Color(255, 18, 17, 30), static_cast<BYTE>(245 * reveal)));
    {
        const auto inner = g.Save();
        auto clip = createRoundedPath(c, 20);
        g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
        // Rays of light behind the gem slowly turn.
        const float cx = c.X + c.Width / 2, cy = c.Y + 78;
        const float turn = ambient ? time * 12 : 20;
        for (int ray = 0; ray < 12; ++ray) {
            Gdiplus::GraphicsPath beam;
            const float a = (turn + static_cast<float>(ray) * 30) * 3.14159265F / 180, spread = .11F;
            beam.AddPolygon(std::array<Gdiplus::PointF, 3>{Gdiplus::PointF(cx, cy),
                Gdiplus::PointF(cx + 260 * std::cos(a - spread), cy + 260 * std::sin(a - spread)),
                Gdiplus::PointF(cx + 260 * std::cos(a + spread), cy + 260 * std::sin(a + spread))}.data(), 3);
            Gdiplus::PathGradientBrush brush(&beam);
            brush.SetCenterPoint(Gdiplus::PointF(cx, cy));
            brush.SetCenterColor(tint(light, static_cast<BYTE>(40 * reveal)));
            Gdiplus::Color edge = tint(light, 0);
            int count = 1;
            brush.SetSurroundColors(&edge, &count);
            g.FillPath(&brush, &beam);
        }
        Gdiplus::LinearGradientBrush wash(Gdiplus::PointF(0, c.Y - 1), Gdiplus::PointF(0, c.Y + 150),
            tint(deep, static_cast<BYTE>(60 * reveal)), tint(deep, 0));
        g.FillRectangle(&wash, c.X, c.Y, c.Width, 150.0F);
        g.Restore(inner);
    }
    drawRoundedRectangle(g, {c.X + .5F, c.Y + .5F, c.Width - 1, c.Height - 1}, 20, tint(light, static_cast<BYTE>(110 * reveal)));

    // The emblem breathes gently.
    const float pulse = ambient ? 1 + .04F * std::sin(time * 3.2F) : 1;
    const float size = 64 * pulse;
    const float cx = c.X + c.Width / 2, cy = c.Y + 78;
    for (int ring = 3; ring >= 1; --ring) {
        const float r = size / 2 + static_cast<float>(ring) * 6;
        Gdiplus::SolidBrush halo(tint(light, static_cast<BYTE>(26 * reveal / static_cast<float>(ring))));
        g.FillEllipse(&halo, cx - r, cy - r, 2 * r, 2 * r);
    }
    drawTierGem(g, {cx - size / 2, cy - size / 2, size, size}, level);
    // Sparkles twinkle around it.
    const struct { float x, y, s, o; } sparks[] = {{-58, -22, 12, 0}, {50, -34, 9, .3F}, {60, 18, 11, .6F}, {-50, 26, 8, .8F}};
    for (const auto& spark : sparks) {
        const float twinkle = ambient ? std::max(0.0F, std::sin((time / 1.4F + spark.o) * 6.2831853F)) : .8F;
        const float s = spark.s * (.5F + .5F * twinkle);
        if (twinkle > .05F)
            Icons::draw(g, Icons::Icon::Sparkle, {cx + spark.x - s / 2, cy + spark.y - s / 2, s, s}, tint(light, static_cast<BYTE>(255 * twinkle * reveal)));
    }

    drawText(g, L"NOVA CONQUISTA", {c.X, c.Y + 124, c.Width, 18}, 11.5F, Gdiplus::FontStyleBold, light,
        Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
    drawDisplayText(g, std::wstring(L"Você chegou no nível ") + RankTier::name(level) + L"!", {c.X + 20, c.Y + 144, c.Width - 40, 34}, 23,
        Theme::TextPrimary, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
    const std::uint64_t hours = model.celebrationSeconds / 3600;
    drawText(g, std::to_wstring(hours) + L" horas jogadas. Seu nome brilha mais no ranking.",
        {c.X + 30, c.Y + 180, c.Width - 60, 20}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextSecondary,
        Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
    // Progress toward the next level, or the top of the ladder.
    const Gdiplus::RectF track{c.X + 80, c.Y + 216, c.Width - 160, 6};
    if (level < RankTier::Levels) {
        fillPill(g, track, Theme::SurfaceSunken);
        const float fraction = RankTier::progress(model.celebrationSeconds);
        if (fraction > .01F) {
            Gdiplus::LinearGradientBrush fill(Gdiplus::PointF(track.X - 1, 0), Gdiplus::PointF(track.GetRight() + 1, 0), deep, light);
            auto bar = createRoundedPath({track.X, track.Y, std::max(track.Height, track.Width * fraction), track.Height}, 3);
            g.FillPath(&fill, bar.get());
        }
        drawText(g, std::wstring(L"Próximo: ") + RankTier::name(level + 1) + L" em " + std::to_wstring(RankTier::startHours(level + 1)) + L" h",
            {c.X, track.Y + 10, c.Width, 18}, 11.5F, Gdiplus::FontStyleRegular, Theme::TextMuted, Gdiplus::StringAlignmentCenter);
    } else {
        drawText(g, L"Nível máximo. Lenda do servidor.", {c.X, track.Y - 4, c.Width, 18}, 12, Gdiplus::FontStyleBold, light,
            Gdiplus::StringAlignmentCenter);
    }
    Buttons::draw(g, Layout::CelebrationButton, L"Show!", visuals(LauncherConstants::CelebrationButtonId), Buttons::Style::Primary,
        Icons::Icon::Sparkle, false, 14);
    g.Restore(saved);
}

void HomeView::drawSpotlight(Gdiplus::Graphics& g, const HomeModel& model, const ControlVisuals& visuals) const
{
    using namespace LauncherConstants;
    const auto& item = model.spotlightItem;
    const float time = model.player.time;
    const bool ambient = model.player.ambient;
    const auto saved = g.Save();
    g.SetClip(Layout::Hero, Gdiplus::CombineModeIntersect);
    // Drops in from the top edge of the landscape, then stays until read or dismissed.
    g.TranslateTransform(0, -(Layout::Spotlight.GetBottom() - Layout::Hero.Y) * (1 - easeOut(model.spotlightReveal)));
    const auto& s = Layout::Spotlight;

    // A violet halo breathes around the card so it is noticed without being in the way.
    const float breath = ambient ? .5F + .5F * std::sin(time * 6.2831853F / 2.6F) : .5F;
    for (int layer = 4; layer >= 1; --layer) {
        const float grow = static_cast<float>(layer) * (2.2F + 1.2F * breath);
        const auto alpha = static_cast<BYTE>((9 + 9 * breath) / static_cast<float>(layer) * 1.7F);
        fillRoundedRectangle(g, {s.X - grow, s.Y - grow, s.Width + 2 * grow, s.Height + 2 * grow}, 16 + grow,
            Gdiplus::Color(alpha, 165, 130, 255));
    }
    fillRoundedRectangle(g, s, 16, Gdiplus::Color(232, 17, 15, 30));
    {
        const auto inner = g.Save();
        auto clip = createRoundedPath(s, 16);
        g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
        // Violet wash from the left edge and an accent rail marking it as news.
        Gdiplus::LinearGradientBrush wash(Gdiplus::PointF(s.X - 1, 0), Gdiplus::PointF(s.X + s.Width * .7F + 1, 0),
            Gdiplus::Color(70, 165, 130, 255), Gdiplus::Color(0, 165, 130, 255));
        g.FillRectangle(&wash, s.X, s.Y, s.Width * .7F, s.Height);
        Gdiplus::LinearGradientBrush rail(Gdiplus::PointF(0, s.Y), Gdiplus::PointF(0, s.GetBottom()), Theme::AccentLight, Theme::Accent);
        g.FillRectangle(&rail, s.X, s.Y, 4.0F, s.Height);
        // An occasional sheen crosses the card.
        if (ambient) {
            const float cycle = std::fmod(time + 1.5F, 5.5F) / 1.3F;
            if (cycle < 1) lightBand(g, s.X - 60 + (s.Width + 120) * easeInOut(cycle), s.Y, s.Height, 90,
                static_cast<BYTE>(40 * std::sin(cycle * 3.14159265F)));
        }
        g.Restore(inner);
    }
    drawRoundedRectangle(g, {s.X + .5F, s.Y + .5F, s.Width - 1, s.Height - 1}, 16, Theme::AccentOutline);

    // Kicker: a solid tag with a live dot, then the category and date.
    const std::wstring tag = item.local ? L"LAUNCHER ATUALIZADO" : L"NOVIDADE";
    const float tagWidth = measureText(g, tag, 10.5F, Gdiplus::FontStyleBold) + 34;
    const Gdiplus::RectF pill{s.X + 20, s.Y + 16, tagWidth, 22};
    fillPill(g, pill, Theme::Accent);
    dot(g, pill.X + 9, pill.Y + 7, Theme::AccentInk, 8);
    if (ambient) pulseRing(g, pill.X + 13, pill.Y + 11, Theme::AccentInk, time, 1.6F);
    drawText(g, tag, {pill.X + 22, pill.Y, pill.Width - 26, pill.Height}, 10.5F, Gdiplus::FontStyleBold, Theme::AccentInk,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    std::wstring meta = item.local ? item.date : item.category + (item.date.empty() ? L"" : L" · " + item.date);
    drawText(g, meta, {pill.GetRight() + 10, pill.Y, s.GetRight() - pill.GetRight() - 30, pill.Height}, 11.5F, Gdiplus::FontStyleBold,
        Theme::AccentLight, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);

    drawDisplayText(g, item.title, {s.X + 20, s.Y + 44, s.Width - 40, 26}, 17, Theme::TextPrimary, Gdiplus::StringAlignmentNear,
        Gdiplus::StringAlignmentCenter);
    drawText(g, item.description, {s.X + 20, s.Y + 72, s.Width - 40, 20}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextSecondary,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);

    Buttons::draw(g, Layout::SpotlightRead, L"Ler agora", visuals(SpotlightReadButtonId), Buttons::Style::Primary,
        Icons::Icon::News, false, 13);
    Buttons::draw(g, Layout::SpotlightDismiss, L"Depois", visuals(SpotlightDismissButtonId), Buttons::Style::Link,
        std::nullopt, false, 12.5F);
    if (model.unreadNews > 1) {
        const std::wstring more = L"+" + std::to_wstring(model.unreadNews - 1) + (model.unreadNews == 2 ? L" novidade" : L" novidades");
        drawText(g, more, {Layout::SpotlightDismiss.GetRight() + 8, Layout::SpotlightRead.Y, s.GetRight() - Layout::SpotlightDismiss.GetRight() - 28,
            Layout::SpotlightRead.Height}, 12, Gdiplus::FontStyleRegular, Theme::TextMuted, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter);
    }
    g.Restore(saved);
}

void HomeView::drawTiles(Gdiplus::Graphics& g, const HomeModel& model) const
{
    const auto& state = *model.player.state;
    const int serverMods = model.serverMods, userMods = model.userMods;
    if (state.statusCards.size() < 3) return;
    struct Tile { const wchar_t* label; StatusCardData data; };
    auto mods = state.statusCards[1];
    if (serverMods >= 0) {
        mods.detail = std::to_wstring(serverMods) + L" do servidor";
        if (userMods > 0) mods.detail += L" · " + std::to_wstring(userMods) + (userMods == 1 ? L" seu" : L" seus");
    }
    const Tile tiles[] = {{L"Servidor", state.statusCards[2]}, {L"Mods", mods}, {L"Instalação", state.statusCards[0]}};
    for (int i = 0; i < 3; ++i) {
        auto b = Layout::tile(i);
        b.Y += std::round(18 * (1 - easeOut((model.entrance - .1F - .08F * static_cast<float>(i)) / .5F)));
        fillRoundedRectangle(g, b, Theme::CardRadius, Theme::Surface);
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius, Theme::Divider);
        dot(g, b.X + 18, b.Y + 21, colorForTone(tiles[i].data.tone));
        if (model.player.motion && tiles[i].data.tone == StatusTone::Information)
            pulseRing(g, b.X + 22, b.Y + 25, colorForTone(tiles[i].data.tone), model.player.time + static_cast<float>(i) * .3F);
        drawText(g, tiles[i].label, {b.X + 32, b.Y + 14, b.Width - 50, 20}, 12, Gdiplus::FontStyleRegular, Theme::TextMuted,
            Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        drawText(g, tiles[i].data.value, {b.X + 18, b.Y + 36, b.Width - 36, 26}, 18, Gdiplus::FontStyleBold, Theme::TextPrimary);
        drawText(g, tiles[i].data.detail, {b.X + 18, b.Y + 64, b.Width - 36, 20}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextSecondary);
    }
}

void HomeView::drawNews(Gdiplus::Graphics& g, const HomeModel& model, const HeroComponent& hero,
                        const ControlVisuals& visuals) const
{
    const auto& state = *model.player.state;
    drawDisplayText(g, L"Novidades", Layout::NewsHeading, 17, Theme::TextPrimary, Gdiplus::StringAlignmentNear,
        Gdiplus::StringAlignmentCenter);
    Buttons::draw(g, {Layout::NewsAll.X + 20, Layout::NewsAll.Y, Layout::NewsAll.Width - 20, Layout::NewsAll.Height}, L"Ver todas",
        visuals(LauncherConstants::NewsAllButtonId), Buttons::Style::Link, std::nullopt, false, 13);
    const float focus[][2] = {{.62F, .4F}, {.15F, .7F}, {.92F, .2F}};
    for (int i = 0; i < LauncherConstants::NewsCardCount; ++i) {
        auto b = Layout::newsCard(i);
        b.Y += std::round(22 * (1 - easeOut((model.entrance - .25F - .08F * static_cast<float>(i)) / .5F)));
        if (static_cast<std::size_t>(i) >= state.news.size()) {
            drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius, Theme::Divider);
            continue;
        }
        const auto& item = state.news[static_cast<std::size_t>(i)];
        const auto visual = visuals(LauncherConstants::NewsCardFirstId + i);
        // Hover lifts the card and casts a soft shadow under it.
        if (visual.hover > .01F) {
            for (int layer = 3; layer >= 1; --layer) {
                const float spread = static_cast<float>(layer) * 3;
                fillRoundedRectangle(g, {b.X - spread + 4, b.Y + 6 + spread, b.Width + 2 * spread - 8, b.Height - 4},
                    Theme::CardRadius + spread, Gdiplus::Color(static_cast<BYTE>(22 * visual.hover), 0, 0, 0));
            }
        }
        b.Y -= std::round(4 * visual.hover);
        fillRoundedRectangle(g, b, Theme::CardRadius, blend(Theme::Surface, Theme::SurfaceElevated, visual.hover * .6F));
        const auto saved = g.Save();
        auto clip = createRoundedPath(b, Theme::CardRadius);
        g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
        hero.drawThumbnail(g, {b.X, b.Y, b.Width, 78}, static_cast<std::size_t>(i), focus[i][0], focus[i][1]);
        g.Restore(saved);
        if (item.unread) {
            const Gdiplus::RectF fresh{b.GetRight() - 62, b.Y + 10, 52, 22};
            fillPill(g, {fresh.X - 2, fresh.Y - 2, fresh.Width + 4, fresh.Height + 4}, Gdiplus::Color(120, 13, 14, 20));
            fillPill(g, fresh, Theme::Accent);
            drawText(g, L"NOVO", fresh, 10.5F, Gdiplus::FontStyleBold, Theme::AccentInk, Gdiplus::StringAlignmentCenter,
                Gdiplus::StringAlignmentCenter);
        }
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius,
            blend(Theme::Divider, Theme::AccentOutline, visual.hover));
        drawText(g, item.category, {b.X + 14, b.Y + 90, b.Width - 28, 16}, 11, Gdiplus::FontStyleBold, Theme::AccentLight);
        drawText(g, item.title, {b.X + 14, b.Y + 110, b.Width - 28, 40}, 13.5F, Gdiplus::FontStyleBold, Theme::TextPrimary);
        drawText(g, item.date, {b.X + 14, b.Y + 156, b.Width - 28, 18}, 11.5F, Gdiplus::FontStyleRegular, Theme::TextMuted);
        if (visual.focused) drawFocusRing(g, b, Theme::CardRadius, Theme::AccentLight);
    }
}

void HomeView::drawPodiumRow(Gdiplus::Graphics& g, const Gdiplus::RectF& line, const RankingRowModel& row, float time, bool ambient)
{
    // Light and deep tone of each medal: gold, silver, bronze.
    static const Gdiplus::Color Medals[3][2] = {
        {Gdiplus::Color(255, 255, 228, 140), Gdiplus::Color(255, 222, 164, 38)},
        {Gdiplus::Color(255, 238, 242, 250), Gdiplus::Color(255, 148, 160, 182)},
        {Gdiplus::Color(255, 250, 196, 158), Gdiplus::Color(255, 186, 106, 60)}};
    const int place = row.rank - 1;
    const auto light = Medals[place][0], deep = Medals[place][1];
    const auto tint = [](const Gdiplus::Color& c, BYTE alpha) { return Gdiplus::Color(alpha, c.GetR(), c.GetG(), c.GetB()); };
    const int level = RankTier::level(row.seconds);
    const float phase = static_cast<float>(place) * 1.3F;

    // Level 2: the row is washed in the medal color.
    if (level >= 2) {
        const auto saved = g.Save();
        auto clip = createRoundedPath(line, 8);
        g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
        Gdiplus::LinearGradientBrush wash(Gdiplus::PointF(line.X, 0), Gdiplus::PointF(line.GetRight(), 0),
            tint(deep, static_cast<BYTE>(level >= 4 ? 70 : 46)), tint(deep, 0));
        g.FillRectangle(&wash, line);
        // Level 4: a sheen sweeps across the row every few seconds.
        if (level >= 4 && ambient) {
            const float cycle = std::fmod(time + phase, 4.2F) / 1.1F;
            if (cycle < 1) lightBand(g, line.X - 40 + (line.Width + 80) * easeInOut(cycle), line.Y, line.Height, 54,
                static_cast<BYTE>(70 * std::sin(cycle * 3.14159265F)));
        }
        g.Restore(saved);
    }
    // Level 5: an outline that shimmers between the medal and the launcher's violet.
    if (level >= 5) {
        const float shift = ambient ? .5F + .5F * std::sin(time * 2.1F + phase) : .5F;
        Gdiplus::LinearGradientBrush edge(Gdiplus::PointF(line.X - line.Width * shift, 0), Gdiplus::PointF(line.GetRight() + line.Width * (1 - shift), 0),
            light, Theme::Accent);
        const Gdiplus::Color colors[] = {tint(light, 230), tint(Theme::Accent, 200), tint(light, 230)};
        const Gdiplus::REAL positions[] = {0, .5F, 1};
        edge.SetInterpolationColors(colors, positions, 3);
        Gdiplus::Pen pen(&edge, 1.3F);
        auto path = createRoundedPath({line.X + .65F, line.Y + .65F, line.Width - 1.3F, line.Height - 1.3F}, 8);
        g.DrawPath(&pen, path.get());
    }

    // The medal: a coin with the position, glowing from level 3.
    const Gdiplus::RectF coin{line.X + 6, line.Y + 3, 20, 20};
    if (level >= 3) {
        const float glow = ambient ? .6F + .4F * std::sin(time * 3.0F + phase) : .8F;
        for (int ring = 3; ring >= 1; --ring) {
            const float grow = static_cast<float>(ring) * 1.6F;
            Gdiplus::SolidBrush halo(tint(light, static_cast<BYTE>(58 * glow / static_cast<float>(ring))));
            g.FillEllipse(&halo, coin.X - grow, coin.Y - grow, coin.Width + 2 * grow, coin.Height + 2 * grow);
        }
    }
    Gdiplus::LinearGradientBrush face(Gdiplus::PointF(0, coin.Y), Gdiplus::PointF(0, coin.GetBottom()), light, deep);
    g.FillEllipse(&face, coin);
    Gdiplus::Pen rim(tint(light, 200), 1);
    g.DrawEllipse(&rim, coin.X + 1.5F, coin.Y + 1.5F, coin.Width - 3, coin.Height - 3);
    drawText(g, std::to_wstring(row.rank), coin, 11, Gdiplus::FontStyleBold, Gdiplus::Color(255, 46, 32, 12),
        Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
    if (row.online) {
        Gdiplus::SolidBrush ring(Theme::Surface);
        g.FillEllipse(&ring, coin.GetRight() - 7, coin.GetBottom() - 7, 10.0F, 10.0F);
        dot(g, coin.GetRight() - 5.5F, coin.GetBottom() - 5.5F, Theme::Success, 7);
    }
    // Level 5: sparkles twinkle around the medal.
    if (level >= 5) {
        const struct { float x, y, size, offset; } sparks[] = {{-3, -4, 7, 0}, {17, -3, 6, .45F}, {19, 15, 5, .8F}};
        for (const auto& spark : sparks) {
            const float twinkle = ambient ? std::max(0.0F, std::sin((time / 1.6F + spark.offset + phase) * 6.2831853F)) : .7F;
            if (twinkle < .05F) continue;
            const float size = spark.size * (.6F + .4F * twinkle);
            Icons::draw(g, Icons::Icon::Sparkle, {coin.X + spark.x + (spark.size - size) / 2, coin.Y + spark.y + (spark.size - size) / 2, size, size},
                tint(light, static_cast<BYTE>(255 * twinkle)));
        }
    }

    // Name and time take the medal color as the level rises; level 4 earns a crown.
    const float nameX = line.X + 44;  // same column as the rows below the podium
    const float timeWidth = 76;
    const float nameRoom = line.Width - 44 - timeWidth - 8 - (level >= 4 ? 16.0F : 0.0F);
    drawText(g, row.name, {nameX, line.Y, nameRoom, line.Height}, 12.5F, Gdiplus::FontStyleBold, level >= 3 ? light : Theme::TextPrimary,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    if (level >= 4) {
        const float nameWidth = std::min(nameRoom, measureText(g, row.name, 12.5F, Gdiplus::FontStyleBold));
        Icons::draw(g, Icons::Icon::Crown, {nameX + nameWidth + 4, line.Y + 7, 12, 12}, deep);
    }
    drawText(g, row.time, {line.GetRight() - timeWidth - 8, line.Y, timeWidth, line.Height}, 12,
        level >= 2 ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, level >= 2 ? blend(Theme::TextSecondary, light, .55F) : Theme::TextSecondary,
        Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter);
}

Gdiplus::RectF HomeView::rankingList()
{
    // Above the divider and the "you" line at the bottom of the card.
    const auto card = Layout::rankingCard();
    return {card.X, card.Y + 4, card.Width, 143};
}

float HomeView::maximumRankingScroll(std::size_t rows)
{
    return std::max(0.0F, static_cast<float>(rows) * RankingRowHeight + 8 - rankingList().Height);
}

void HomeView::drawRanking(Gdiplus::Graphics& g, const HomeModel& model) const
{
    drawDisplayText(g, L"Top jogadores", Layout::RankingHeading, 17, Theme::TextPrimary, Gdiplus::StringAlignmentNear,
        Gdiplus::StringAlignmentCenter);
    if (model.ranking.size() > 5)
        drawText(g, std::to_wstring(model.ranking.size()) + L" jogadores", Layout::RankingHeading, 12, Gdiplus::FontStyleRegular,
            Theme::TextMuted, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter);
    auto b = Layout::rankingCard();
    b.Y += std::round(22 * (1 - easeOut((model.entrance - .41F) / .5F)));
    fillRoundedRectangle(g, b, Theme::CardRadius, Theme::Surface);
    drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius, Theme::Divider);
    if (model.ranking.empty()) {
        drawText(g, model.rankingNotice, {b.X + 20, b.Y, b.Width - 40, b.Height}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextMuted,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
        return;
    }
    // The list moves with the card's entrance; scrolling happens inside it.
    auto list = rankingList();
    list.Y = b.Y + (list.Y - Layout::rankingCard().Y);
    const std::size_t rows = model.ranking.size();
    const bool scrolls = maximumRankingScroll(rows) > 0;
    const float left = b.X + 8, width = b.Width - 16 - (scrolls ? 6.0F : 0.0F);
    const float time = model.player.time;
    const bool ambient = model.player.ambient;
    const float scroll = std::clamp(model.rankingScroll, 0.0F, maximumRankingScroll(rows));
    const auto clipState = g.Save();
    g.SetClip(list, Gdiplus::CombineModeIntersect);
    for (std::size_t i = 0; i < rows; ++i) {
        const auto& row = model.ranking[i];
        const float y = list.Y + 4 + static_cast<float>(i) * RankingRowHeight - scroll;
        if (y + RankingRowHeight < list.Y || y > list.GetBottom()) continue;
        const Gdiplus::RectF line{left, y, width, 26};
        if (row.you) fillRoundedRectangle(g, line, 8, Theme::AccentTint);
        // Podium positions use the medal colors; anything below keeps the plain row.
        const bool podium = i < 3 && row.rank >= 1 && row.rank <= 3;
        if (!podium) {
            drawText(g, std::to_wstring(row.rank), {left + 10, y, 20, 26}, 12.5F, Gdiplus::FontStyleBold, Theme::TextMuted,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
            if (row.online) dot(g, left + 30, y + 9, Theme::Success);
            drawText(g, row.name, {left + 44, y, width - 44 - 80, 26}, 12.5F, Gdiplus::FontStyleBold, Theme::TextPrimary,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
            drawText(g, row.time, {left + width - 84, y, 76, 26}, 12, Gdiplus::FontStyleRegular, Theme::TextSecondary,
                Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter);
            continue;
        }
        drawPodiumRow(g, line, row, time, ambient);
    }
    if (scrolls) {
        // Rows fade at the edge that has more players behind it.
        const auto fade = [&](float top, bool down) {
            const Gdiplus::Color surface = Theme::Surface, clear(0, surface.GetR(), surface.GetG(), surface.GetB());
            Gdiplus::LinearGradientBrush brush(Gdiplus::PointF(0, top - 1), Gdiplus::PointF(0, top + 15), down ? clear : surface, down ? surface : clear);
            g.FillRectangle(&brush, list.X, top, list.Width, 14.0F);
        };
        if (scroll > .5F) fade(list.Y, false);
        if (scroll < maximumRankingScroll(rows) - .5F) fade(list.GetBottom() - 14, true);
    }
    g.Restore(clipState);
    if (scrolls) {
        const float content = static_cast<float>(rows) * RankingRowHeight + 8;
        const float thumb = std::max(20.0F, list.Height * list.Height / content);
        const float travel = (list.Height - 8 - thumb) * scroll / maximumRankingScroll(rows);
        fillPill(g, {b.GetRight() - 9, list.Y + 4 + travel, 3, thumb}, Theme::Border);
    }
    Gdiplus::Pen line(Theme::Divider);
    g.DrawLine(&line, b.X + 16, b.Y + 147, b.GetRight() - 16, b.Y + 147);
    drawText(g, model.rankingYou, {b.X + 18, b.Y + 150, b.Width - 36, 30}, 12, Gdiplus::FontStyleRegular, Theme::TextSecondary,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
}

void HomeView::drawPlayerCard(Gdiplus::Graphics& g, const PlayerCardModel& m, const ControlVisuals& visuals)
{
    using namespace LauncherConstants;
    const auto& state = *m.state;
    const auto& aside = Layout::Aside;
    fillRoundedRectangle(g, aside, Theme::PanelRadius, Theme::Surface);
    drawRoundedRectangle(g, {aside.X + .5F, aside.Y + .5F, aside.Width - 1, aside.Height - 1}, Theme::PanelRadius, Theme::Divider);
    const auto tone = m.statusHeadline.empty() ? statusColor(state, m.playing) : Theme::Success;
    drawAvatar(g, Layout::Avatar, tone, m.face ? *m.face : SkinLookup::defaultFace(), m.skinLoading ? m.time / 1.2F : -1.0F);
    if (m.motion && m.playing)
        pulseRing(g, Layout::Avatar.GetRight() - 6, Layout::Avatar.GetBottom() - 6, tone, m.time, 2.2F);

    drawText(g, m.originalMode ? L"CONTA MICROSOFT" : L"JOGANDO COMO", Layout::NicknameKicker, 11, Gdiplus::FontStyleBold, Theme::TextMuted);
    if (m.originalMode) {
        drawDisplayText(g, m.microsoftName.empty() ? L"Entrar ao jogar" : m.microsoftName,
            {Layout::NicknameField.X, Layout::NicknameField.Y + 2, Layout::NicknameField.Width, 36}, m.microsoftName.empty() ? 17.0F : 22.0F,
            m.microsoftName.empty() ? Theme::TextSecondary : Theme::TextPrimary, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        drawText(g, m.microsoftName.empty() ? L"Requer Minecraft: Java Edition" : L"Conta conectada", Layout::NicknameStatus,
            12, Gdiplus::FontStyleRegular, m.microsoftName.empty() ? Theme::TextMuted : Theme::Success);
    } else {
        const auto editVisual = visuals(NicknameInputId);
        Gdiplus::Pen underline(editVisual.focused ? Theme::Accent : Gdiplus::Color(255, 59, 58, 82), editVisual.focused ? 2.0F : 1.0F);
        if (!editVisual.focused) underline.SetDashStyle(Gdiplus::DashStyleDash);
        g.DrawLine(&underline, Layout::NicknameField.X, Layout::NicknameField.GetBottom(), Layout::NicknameField.GetRight(),
            Layout::NicknameField.GetBottom());
        if (!m.previewNickname.empty())
            drawDisplayText(g, m.previewNickname, Layout::NicknameEdit, 22, Theme::TextPrimary, Gdiplus::StringAlignmentNear,
                Gdiplus::StringAlignmentCenter);
        Buttons::drawIcon(g, Layout::EditNickname, Icons::Icon::Pencil, visuals(EditNicknameButtonId), false, 16);
        const bool ok = m.nicknameValid && !m.nicknameEmpty;
        if (ok) Icons::draw(g, Icons::Icon::Check, {Layout::NicknameStatus.X, Layout::NicknameStatus.Y + 3, 12, 12}, Theme::Success, 1.8F);
        drawText(g, ok ? (m.skinFound ? L"Skin da conta original" : L"Nickname válido") : m.nicknameEmpty ? L"Digite seu nickname" : L"Use 3 a 16 letras, números ou _",
            {Layout::NicknameStatus.X + (ok ? 18.0F : 0.0F), Layout::NicknameStatus.Y, Layout::NicknameStatus.Width, 18}, 12,
            Gdiplus::FontStyleRegular, ok ? Theme::Success : m.nicknameEmpty ? Theme::TextMuted : Theme::Error);
    }

    drawText(g, L"Como entrar", Layout::ModeLabel, 12.5F, Gdiplus::FontStyleBold, Theme::TextSecondary);
    // One selection travels between the two options instead of jumping.
    const float travel = easeInOut(m.identitySelection);
    const Gdiplus::RectF selection{Layout::OfflineMode.X + (Layout::OriginalMode.X - Layout::OfflineMode.X) * travel,
        Layout::OfflineMode.Y, Layout::OfflineMode.Width, Layout::OfflineMode.Height};
    const struct { const Gdiplus::RectF* bounds; int id; bool selected; Icons::Icon icon; const wchar_t* title; const wchar_t* description; } modes[] = {
        {&Layout::OfflineMode, OfflineButtonId, !m.originalMode, Icons::Icon::User, L"Nickname", L"Sem login. Para o servidor do grupo."},
        {&Layout::OriginalMode, OriginalButtonId, m.originalMode, Icons::Icon::Key, L"Conta original", L"Entrar com Microsoft e usar sua skin."}};
    for (const auto& mode : modes) {
        const auto& b = *mode.bounds;
        const auto v = visuals(mode.id);
        fillRoundedRectangle(g, b, 14, blend(Gdiplus::Color(0, 30, 31, 44), Theme::SurfaceElevated, v.hover));
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, 14, blend(Theme::OutlineSubtle, Theme::Border, v.hover));
    }
    fillRoundedRectangle(g, selection, 14, Theme::AccentTint);
    drawRoundedRectangle(g, {selection.X + .5F, selection.Y + .5F, selection.Width - 1, selection.Height - 1}, 14, Theme::Accent);
    for (const auto& mode : modes) {
        const auto& b = *mode.bounds;
        const auto v = visuals(mode.id);
        // Content color follows how much of the selection currently covers this card.
        const float cover = std::clamp(1 - std::abs(selection.X - b.X) / b.Width, 0.0F, 1.0F);
        Icons::draw(g, mode.icon, {b.X + 12, b.Y + 12, 18, 18}, blend(Theme::TextMuted, Theme::AccentLight, cover), 1.8F);
        drawText(g, mode.title, {b.X + 12, b.Y + 36, b.Width - 20, 18}, 13, Gdiplus::FontStyleBold,
            v.enabled ? Theme::TextPrimary : Theme::TextMuted);
        drawText(g, mode.description, {b.X + 12, b.Y + 56, b.Width - 18, 36}, 11.5F, Gdiplus::FontStyleRegular,
            blend(Theme::TextMuted, Theme::TextSecondary, cover));
        if (v.focused) drawFocusRing(g, b, 14, Theme::AccentLight);
    }

    const wchar_t* statLabels[] = {L"Última partida", L"Tempo jogado"};
    const std::wstring statValues[] = {m.lastPlayed, m.totalPlayed};
    for (int i = 0; i < 2; ++i) {
        const auto b = Layout::stat(i);
        fillRoundedRectangle(g, b, 12, Theme::SurfaceSunken);
        drawText(g, statLabels[i], {b.X + 12, b.Y + 10, b.Width - 24, 16}, 11.5F, Gdiplus::FontStyleRegular, Theme::TextMuted);
        drawText(g, statValues[i], {b.X + 12, b.Y + 28, b.Width - 24, 22}, 14, Gdiplus::FontStyleBold, Theme::TextPrimary);
        if (i == 1 && m.tier > 0) {
            // The player's level: its gem, and a thin bar toward the next one.
            drawTierGem(g, {b.GetRight() - 26, b.Y + 10, 15, 15}, m.tier);
            const Gdiplus::RectF track{b.X + 12, b.Y + b.Height - 8, b.Width - 24, 3};
            fillPill(g, track, Gdiplus::Color(255, 40, 41, 56));
            if (m.tierProgress > .01F)
                fillPill(g, {track.X, track.Y, std::max(3.0F, track.Width * m.tierProgress), track.Height}, tierLight(m.tier));
        }
    }

    {
        const auto& box = Layout::InstallSummary;
        drawRoundedRectangle(g, {box.X + .5F, box.Y + .5F, box.Width - 1, box.Height - 1}, 12, Theme::OutlineSubtle);
        // The version row is a button that opens the Versions page.
        const auto versionRow = visuals(VersionChangeButtonId);
        if (versionRow.hover > .01F)
            fillRoundedRectangle(g, Layout::VersionRow, 9, blend(Gdiplus::Color(0, 30, 31, 44), Theme::SurfaceElevated, versionRow.hover));
        if (versionRow.focused) drawFocusRing(g, Layout::VersionRow, 9, Theme::AccentLight);
        Icons::draw(g, Icons::Icon::ChevronRight, {box.GetRight() - 27, box.Y + 13, 14, 14}, blend(Theme::TextMuted, Theme::AccentLight, versionRow.hover), 1.8F);
        const wchar_t* labels[] = {L"Versão", L"Memória", L"Mods"};
        const std::wstring values[] = {m.versionSummary, m.memorySummary, m.modsSummary.empty() ? std::wstring(L"—") : m.modsSummary};
        Gdiplus::Pen line(Theme::Divider);
        for (int i = 0; i < 3; ++i) {
            const float y = box.Y + 4 + static_cast<float>(i) * 33.3F;
            if (i > 0) g.DrawLine(&line, box.X + 14, y, box.GetRight() - 14, y);
            drawText(g, labels[i], {box.X + 14, y, 90, 33}, 12, Gdiplus::FontStyleRegular, Theme::TextMuted,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
            drawText(g, values[i], {box.X + 90, y, box.Width - (i == 0 ? 122 : 104), 33}, 12.5F, Gdiplus::FontStyleBold, Theme::TextPrimary,
                Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter);
        }
    }

    dot(g, Layout::StatusLine.X, Layout::StatusLine.Y + 8, tone);
    const bool working = !state.playEnabled && state.operation != LauncherOperation::UpdateAvailable;
    if (m.motion && (m.playing || working)) pulseRing(g, Layout::StatusLine.X + 4, Layout::StatusLine.Y + 12, tone, m.time);
    drawText(g, m.playing ? L"Jogando agora" : !m.statusHeadline.empty() ? m.statusHeadline : state.headline, {Layout::StatusLine.X + 16, Layout::StatusLine.Y,
        Layout::StatusLine.Width - 16, Layout::StatusLine.Height}, 14, Gdiplus::FontStyleBold, Theme::TextPrimary,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    const std::wstring message = !m.updateWarning.empty() && state.playEnabled ? m.updateWarning :
        !m.statusMessage.empty() ? m.statusMessage : state.operationMessage;
    drawText(g, message, Layout::StatusMessage, 12.5F, Gdiplus::FontStyleRegular,
        !m.statusMessage.empty() ? Theme::TextSecondary :
        isLauncherError(state.operation) ? Theme::Danger :
        state.operation == LauncherOperation::OfflineReady ? Theme::Warning : Theme::TextSecondary);
    if (!m.updateWarning.empty() && state.playEnabled)
        Buttons::draw(g, Layout::UpdateLink, L"Baixar launcher", visuals(UpdateDownloadButtonId), Buttons::Style::Link,
            Icons::Icon::Download, false, 12.5F);
    else if (m.serverOffline && state.playEnabled)
        Buttons::draw(g, Layout::UpdateLink, L"Tentar conectar", visuals(RetryServerButtonId), Buttons::Style::Link,
            Icons::Icon::Refresh, false, 12.5F);

    drawPlayButton(g, m, visuals);

    const struct { int id; const wchar_t* label; } quick[] = {
        {QuickFolderButtonId, L"Pasta do jogo"}, {QuickRepairButtonId, L"Reparar"}, {QuickLogsButtonId, L"Logs"}};
    for (int i = 0; i < 3; ++i)
        Buttons::draw(g, Layout::quickAction(i), quick[i].label, visuals(quick[i].id), Buttons::Style::Ghost, std::nullopt, false, 12.5F);
}

void HomeView::drawPlayButton(Gdiplus::Graphics& g, const PlayerCardModel& m, const ControlVisuals& visuals)
{
    using namespace LauncherConstants;
    const auto& state = *m.state;
    const auto playVisual = visuals(PlayButtonId);
    const auto menuVisual = visuals(PlayMenuButtonId);
    const bool enabled = state.playEnabled;
    const bool ready = enabled && (state.operation == LauncherOperation::ReadyToPlay || state.operation == LauncherOperation::NeedsInstallation ||
        state.operation == LauncherOperation::OfflineReady);
    const float hover = enabled ? std::max(playVisual.hover, playVisual.focused ? 1.0F : 0.0F) : 0.0F;

    // A soft violet halo breathes around the split button while it waits for a click.
    const Gdiplus::RectF group{Layout::Play.X, Layout::Play.Y, Layout::PlayMenu.GetRight() - Layout::Play.X, Layout::Play.Height};
    if (ready && m.ambient) {
        const float breath = .5F + .5F * std::sin(m.time * 6.2831853F / 2.8F);
        for (int layer = 4; layer >= 1; --layer) {
            // At most 13.5 logical pixels, so the glow never reaches the quick actions below.
            const float grow = static_cast<float>(layer) * (2 + breath) + hover * 1.5F;
            const auto alpha = static_cast<BYTE>((10 + 10 * breath + 12 * hover) / static_cast<float>(layer) * 1.6F);
            fillRoundedRectangle(g, {group.X - grow, group.Y - grow, group.Width + 2 * grow, group.Height + 2 * grow},
                14 + grow, Gdiplus::Color(alpha, 165, 130, 255));
        }
    }

    // Pressing compresses the button inside its own hit area.
    const float press = enabled ? std::clamp(m.playPress, 0.0F, 1.0F) : 0.0F;
    const Gdiplus::RectF play{Layout::Play.X + press * 2, Layout::Play.Y + press * 2, Layout::Play.Width - press * 4, Layout::Play.Height - press * 4};
    const auto playFill = enabled ? blend(Theme::Accent, Theme::AccentLight, playVisual.pressed ? 1.0F : hover * .7F) :
        Gdiplus::Color(255, 36, 35, 58);
    fillRoundedRectangle(g, play, 14, 6, 6, 14, playFill);
    {
        const auto saved = g.Save();
        auto clip = createRoundedPath(play, 14, 6, 6, 14);
        g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
        if (!enabled && m.progress >= 0) {
            const float fraction = std::clamp(m.progress, 0.0F, 100.0F) / 100;
            if (fraction > 0) {
                // Filled part brightens toward its leading edge, which carries a soft glow.
                const float edge = play.X + play.Width * fraction;
                Gdiplus::LinearGradientBrush fill(Gdiplus::PointF(play.X - 1, 0), Gdiplus::PointF(edge + 1, 0),
                    Gdiplus::Color(255, 52, 42, 98), Gdiplus::Color(255, 104, 80, 190));
                g.FillRectangle(&fill, play.X, play.Y, edge - play.X, play.Height);
                lightBand(g, edge - 10, play.Y, play.Height, 40, 70);
                Gdiplus::SolidBrush line(Gdiplus::Color(210, 201, 182, 255));
                g.FillRectangle(&line, edge - 2, play.Y, 2.0F, play.Height);
                // A glint keeps travelling over the filled part so slow downloads still feel alive.
                if (m.motion && edge - play.X > 40)
                    lightBand(g, play.X + (edge - play.X) * easeInOut(std::fmod(m.time / 1.8F, 1.0F)), play.Y, play.Height, 46, 45);
            } else {
                // Unknown duration: two soft comets glide across, eased so they never look jerky.
                for (int comet = 0; comet < 2; ++comet) {
                    const float t = std::fmod(m.time / 1.7F + static_cast<float>(comet) * .5F, 1.0F);
                    const float center = play.X - play.Width * .3F + play.Width * 1.6F * easeInOut(t);
                    const float width = play.Width * (comet == 0 ? .55F : .35F);
                    Gdiplus::LinearGradientBrush band(Gdiplus::PointF(center - width / 2, 0), Gdiplus::PointF(center + width / 2, 0),
                        Gdiplus::Color(0, 98, 78, 186), Gdiplus::Color(0, 98, 78, 186));
                    const Gdiplus::Color colors[] = {Gdiplus::Color(0, 98, 78, 186),
                        Gdiplus::Color(static_cast<BYTE>(comet == 0 ? 190 : 120), 98, 78, 186), Gdiplus::Color(0, 98, 78, 186)};
                    const Gdiplus::REAL positions[] = {0, .5F, 1};
                    band.SetInterpolationColors(colors, positions, 3);
                    g.FillRectangle(&band, center - width / 2, play.Y, width, play.Height);
                }
            }
        }
        if (enabled) {
            // Light that follows the pointer across the surface.
            if (hover > .01F) {
                Gdiplus::GraphicsPath glow;
                const float cx = play.X + play.Width * std::clamp(m.playLight, 0.0F, 1.0F);
                glow.AddEllipse(cx - 70, play.Y - 40, 140.0F, play.Height + 80);
                Gdiplus::PathGradientBrush light(&glow);
                light.SetCenterColor(Gdiplus::Color(static_cast<BYTE>(70 * hover), 255, 255, 255));
                Gdiplus::Color edge(0, 255, 255, 255);
                int count = 1;
                light.SetSurroundColors(&edge, &count);
                g.FillPath(&light, &glow);
            }
            // One sweep when the pointer arrives; an occasional idle sheen otherwise.
            if (m.playSweep < 1) {
                const float t = easeInOut(m.playSweep);
                lightBand(g, play.X - 50 + (play.Width + 100) * t, play.Y, play.Height, 70, static_cast<BYTE>(110 * std::sin(t * 3.14159265F)));
            } else if (ready && m.ambient) {
                const float cycle = std::fmod(m.time, 6.5F) / 1.2F;
                if (cycle < 1) lightBand(g, play.X - 40 + (play.Width + 80) * easeInOut(cycle), play.Y, play.Height, 46,
                    static_cast<BYTE>(70 * std::sin(cycle * 3.14159265F)));
            }
            // Click ripple from the center.
            if (m.playPulse > 0) {
                const float spread = 1 - m.playPulse;
                const float radius = 12 + spread * play.Width * .7F;
                Gdiplus::SolidBrush ripple(Gdiplus::Color(static_cast<BYTE>(90 * m.playPulse), 255, 255, 255));
                g.FillEllipse(&ripple, play.X + play.Width / 2 - radius, play.Y + play.Height / 2 - radius, 2 * radius, 2 * radius);
            }
        }
        g.Restore(saved);
    }
    std::wstring label = !m.playLabel.empty() ? m.playLabel : state.primaryActionLabel.empty() ? std::wstring(L"JOGAR") : state.primaryActionLabel;
    if (!enabled && m.progress > 0 && label.find(L'%') == std::wstring::npos)
        label += L" " + std::to_wstring(static_cast<int>(std::floor(m.progress))) + L"%";
    const auto ink = enabled ? Theme::AccentInk : Gdiplus::Color(255, 230, 225, 255);
    const float labelWidth = measureText(g, label, 15, Gdiplus::FontStyleBold);
    const bool working = !enabled && m.progress >= 0;
    const float iconWidth = ready || working ? 26.0F : 0.0F;
    const float startX = play.X + (play.Width - labelWidth - iconWidth) / 2;
    if (working) {
        // Spinner: an arc that turns while its length breathes, like a native progress ring.
        const Gdiplus::RectF ring{startX + 1, play.Y + play.Height / 2 - 7, 14, 14};
        Gdiplus::Pen track(Gdiplus::Color(40, ink.GetR(), ink.GetG(), ink.GetB()), 2.2F);
        g.DrawEllipse(&track, ring);
        Gdiplus::Pen arc(Gdiplus::Color(255, ink.GetR(), ink.GetG(), ink.GetB()), 2.2F);
        arc.SetStartCap(Gdiplus::LineCapRound);
        arc.SetEndCap(Gdiplus::LineCapRound);
        const float head = std::fmod(m.time * 420, 360.0F);
        const float sweep = 50 + 200 * (.5F + .5F * std::sin(m.time * 3.4F));
        g.DrawArc(&arc, ring, head, sweep);
    }
    // The play icon leans forward on hover, leaving a faint trail.
    if (ready) {
        const float nudge = 4 * easeOut(hover);
        if (nudge > .1F)
            Icons::draw(g, Icons::Icon::Play, {startX + nudge * .4F, play.Y + play.Height / 2 - 8, 16, 16},
                Gdiplus::Color(70, ink.GetR(), ink.GetG(), ink.GetB()));
        Icons::draw(g, Icons::Icon::Play, {startX + nudge, play.Y + play.Height / 2 - 8, 16, 16}, ink);
    }
    drawText(g, label, {startX + iconWidth, play.Y, play.GetRight() - startX - iconWidth, play.Height}, 15, Gdiplus::FontStyleBold, ink,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    if (playVisual.focused) drawFocusRing(g, Layout::Play, 12, Theme::AccentInk);

    const auto menuFill = menuVisual.enabled ? blend(Theme::Accent, Theme::AccentLight, menuVisual.pressed ? 1.0F : menuVisual.hover * .7F) :
        Gdiplus::Color(255, 36, 35, 58);
    fillRoundedRectangle(g, Layout::PlayMenu, 6, 14, 14, 6, menuFill);
    // The chevron dips a little on hover, hinting that a menu opens below.
    Icons::draw(g, Icons::Icon::ChevronDown, {Layout::PlayMenu.X + 21, Layout::PlayMenu.Y + 20 + 2 * easeOut(menuVisual.hover), 18, 18},
        menuVisual.enabled ? Theme::AccentInk : Theme::TextMuted, 2);
    if (menuVisual.focused) drawFocusRing(g, Layout::PlayMenu, 12, Theme::AccentInk);
}
