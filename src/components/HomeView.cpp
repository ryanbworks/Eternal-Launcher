#include "components/HomeView.h"

#include "components/HeroComponent.h"
#include "components/Layout.h"
#include "constants/LauncherConstants.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <algorithm>
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

void HomeView::drawAvatar(Gdiplus::Graphics& g, const Gdiplus::RectF& b, const Gdiplus::Color& status, bool blink)
{
    // Generic pixel head: hood, face, eyes and mouth on an 8 x 8 grid.
    fillRoundedRectangle(g, b, 18, Gdiplus::Color(255, 47, 40, 80));
    const Gdiplus::RectF face{b.X + 3, b.Y + 3, b.Width - 6, b.Height - 6};
    const auto saved = g.Save();
    auto clip = createRoundedPath(face, 15);
    g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
    const float cell = face.Width / 8;
    const auto block = [&](int x, int y, int w, int h, Gdiplus::Color color) {
        Gdiplus::SolidBrush brush(color);
        g.FillRectangle(&brush, face.X + static_cast<float>(x) * cell, face.Y + static_cast<float>(y) * cell,
            static_cast<float>(w) * cell + .5F, static_cast<float>(h) * cell + .5F);
    };
    block(0, 0, 8, 8, Gdiplus::Color(255, 42, 40, 59));
    block(0, 0, 8, 3, Gdiplus::Color(255, 59, 47, 99));
    block(0, 3, 1, 5, Gdiplus::Color(255, 59, 47, 99));
    block(7, 3, 1, 5, Gdiplus::Color(255, 59, 47, 99));
    block(1, 3, 6, 5, Gdiplus::Color(255, 201, 154, 118));
    block(1, 2, 6, 1, Theme::Accent);
    if (blink) {
        // Eyelids: a skin-toned row with a darker lash line.
        block(2, 4, 4, 1, Gdiplus::Color(255, 201, 154, 118));
        Gdiplus::SolidBrush lash(Gdiplus::Color(255, 138, 90, 67));
        g.FillRectangle(&lash, face.X + 2 * cell, face.Y + 4.6F * cell, 4 * cell, cell * .3F);
    } else {
        block(2, 4, 1, 1, Theme::TextPrimary);
        block(5, 4, 1, 1, Theme::TextPrimary);
        block(3, 4, 1, 1, Gdiplus::Color(255, 109, 76, 209));
        block(4, 4, 1, 1, Gdiplus::Color(255, 109, 76, 209));
    }
    block(3, 6, 2, 1, Gdiplus::Color(255, 138, 90, 67));
    g.Restore(saved);
    Gdiplus::SolidBrush ring(Theme::Surface);
    g.FillEllipse(&ring, b.GetRight() - 15, b.GetBottom() - 15, 18.0F, 18.0F);
    Gdiplus::SolidBrush indicator(status);
    g.FillEllipse(&indicator, b.GetRight() - 12, b.GetBottom() - 12, 12.0F, 12.0F);
}

void HomeView::draw(Gdiplus::Graphics& g, const HomeModel& model, const HeroComponent& hero,
                    const ControlVisuals& visuals, bool drawLeftColumn) const
{
    if (drawLeftColumn) {
        hero.draw(g, Layout::Hero, model.reveal, model.energy, model.phase, model.drift, model.chips);
        if (model.toast && model.toastReveal > .01F) {
            // The toast drops in from the top edge of the landscape.
            const auto saved = g.Save();
            g.SetClip(Layout::Hero, Gdiplus::CombineModeIntersect);
            g.TranslateTransform(0, -(Layout::Toast.GetBottom() - Layout::Hero.Y) * (1 - easeOut(model.toastReveal)));
            const auto& t = Layout::Toast;
            fillRoundedRectangle(g, t, 12, Gdiplus::Color(214, 13, 14, 20));
            drawRoundedRectangle(g, {t.X + .5F, t.Y + .5F, t.Width - 1, t.Height - 1}, 12, Theme::GlassOutline);
            Gdiplus::SolidBrush badge(Theme::SuccessTint);
            g.FillEllipse(&badge, t.X + 12, t.Y + 10, 20.0F, 20.0F);
            Icons::draw(g, Icons::Icon::Check, {t.X + 16, t.Y + 14, 12, 12}, Theme::Success, 1.8F);
            drawText(g, model.toastText, {t.X + 40, t.Y, 170, t.Height}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextPrimary,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
            Buttons::draw(g, Layout::ToastButton, L"Ver novidades", visuals(LauncherConstants::ToastNewsButtonId),
                Buttons::Style::Link, std::nullopt, false, 12.5F);
            g.Restore(saved);
        }
        drawTiles(g, model);
        drawNews(g, model, hero, visuals);
    }
    drawPlayerCard(g, model.player, visuals);
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
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius,
            blend(Theme::Divider, Theme::AccentOutline, visual.hover));
        drawText(g, item.category, {b.X + 14, b.Y + 90, b.Width - 28, 16}, 11, Gdiplus::FontStyleBold, Theme::AccentLight);
        drawText(g, item.title, {b.X + 14, b.Y + 110, b.Width - 28, 40}, 13.5F, Gdiplus::FontStyleBold, Theme::TextPrimary);
        drawText(g, item.date, {b.X + 14, b.Y + 156, b.Width - 28, 18}, 11.5F, Gdiplus::FontStyleRegular, Theme::TextMuted);
        if (visual.focused) drawFocusRing(g, b, Theme::CardRadius, Theme::AccentLight);
    }
}

void HomeView::drawPlayerCard(Gdiplus::Graphics& g, const PlayerCardModel& m, const ControlVisuals& visuals)
{
    using namespace LauncherConstants;
    const auto& state = *m.state;
    const auto& aside = Layout::Aside;
    fillRoundedRectangle(g, aside, Theme::PanelRadius, Theme::Surface);
    drawRoundedRectangle(g, {aside.X + .5F, aside.Y + .5F, aside.Width - 1, aside.Height - 1}, Theme::PanelRadius, Theme::Divider);
    const auto tone = statusColor(state, m.playing);
    // Blinks for a moment every few seconds while the window is in front.
    drawAvatar(g, Layout::Avatar, tone, m.ambient && std::fmod(m.time, 4.7F) < .14F);
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
        drawText(g, ok ? L"Nickname válido" : m.nicknameEmpty ? L"Digite seu nickname" : L"Use 3 a 16 letras, números ou _",
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
    }

    {
        const auto& box = Layout::InstallSummary;
        drawRoundedRectangle(g, {box.X + .5F, box.Y + .5F, box.Width - 1, box.Height - 1}, 12, Theme::OutlineSubtle);
        const wchar_t* labels[] = {L"Versão", L"Memória", L"Mods"};
        const std::wstring values[] = {m.versionSummary, m.memorySummary, m.modsSummary.empty() ? std::wstring(L"—") : m.modsSummary};
        Gdiplus::Pen line(Theme::Divider);
        for (int i = 0; i < 3; ++i) {
            const float y = box.Y + 4 + static_cast<float>(i) * 33.3F;
            if (i > 0) g.DrawLine(&line, box.X + 14, y, box.GetRight() - 14, y);
            drawText(g, labels[i], {box.X + 14, y, 90, 33}, 12, Gdiplus::FontStyleRegular, Theme::TextMuted,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
            drawText(g, values[i], {box.X + 90, y, box.Width - 104, 33}, 12.5F, Gdiplus::FontStyleBold, Theme::TextPrimary,
                Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter);
        }
    }

    dot(g, Layout::StatusLine.X, Layout::StatusLine.Y + 8, tone);
    const bool working = !state.playEnabled && state.operation != LauncherOperation::UpdateAvailable;
    if (m.motion && (m.playing || working)) pulseRing(g, Layout::StatusLine.X + 4, Layout::StatusLine.Y + 12, tone, m.time);
    drawText(g, m.playing ? L"Jogando agora" : state.headline, {Layout::StatusLine.X + 16, Layout::StatusLine.Y,
        Layout::StatusLine.Width - 16, Layout::StatusLine.Height}, 14, Gdiplus::FontStyleBold, Theme::TextPrimary,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    const std::wstring message = !m.updateWarning.empty() && state.playEnabled ? m.updateWarning : state.operationMessage;
    drawText(g, message, Layout::StatusMessage, 12.5F, Gdiplus::FontStyleRegular,
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
    std::wstring label = state.primaryActionLabel.empty() ? L"JOGAR" : state.primaryActionLabel;
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
        Gdiplus::Pen track(Gdiplus::Color(55, ink.GetR(), ink.GetG(), ink.GetB()), 2);
        g.DrawEllipse(&track, ring);
        Gdiplus::Pen arc(Gdiplus::Color(235, ink.GetR(), ink.GetG(), ink.GetB()), 2);
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
