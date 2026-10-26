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
        if (state.operation == LauncherOperation::NeedsInstallation) return Theme::Warning;
        return state.playEnabled ? Theme::Success : Theme::Accent;
    }

    void dot(Gdiplus::Graphics& g, float x, float y, const Gdiplus::Color& color, float size = 8)
    {
        Gdiplus::SolidBrush brush(color);
        g.FillEllipse(&brush, x, y, size, size);
    }
}

void HomeView::drawAvatar(Gdiplus::Graphics& g, const Gdiplus::RectF& b, const Gdiplus::Color& status)
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
    block(2, 4, 1, 1, Theme::TextPrimary);
    block(5, 4, 1, 1, Theme::TextPrimary);
    block(3, 4, 1, 1, Gdiplus::Color(255, 109, 76, 209));
    block(4, 4, 1, 1, Gdiplus::Color(255, 109, 76, 209));
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
    const auto& state = *model.player.state;
    if (drawLeftColumn) {
        hero.draw(g, Layout::Hero, model.reveal, model.energy, model.phase, model.drift, model.chips);
        if (model.toast) {
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
        }
        drawTiles(g, state, model.serverMods, model.userMods);
        drawNews(g, state, hero, visuals);
    }
    drawPlayerCard(g, model.player, visuals);
}

void HomeView::drawTiles(Gdiplus::Graphics& g, const LauncherViewState& state, int serverMods, int userMods) const
{
    if (state.statusCards.size() < 3) return;
    struct Tile { const wchar_t* label; StatusCardData data; };
    auto mods = state.statusCards[1];
    if (serverMods >= 0) {
        mods.detail = std::to_wstring(serverMods) + L" do servidor";
        if (userMods > 0) mods.detail += L" · " + std::to_wstring(userMods) + (userMods == 1 ? L" seu" : L" seus");
    }
    const Tile tiles[] = {{L"Servidor", state.statusCards[2]}, {L"Mods", mods}, {L"Instalação", state.statusCards[0]}};
    for (int i = 0; i < 3; ++i) {
        const auto b = Layout::tile(i);
        fillRoundedRectangle(g, b, Theme::CardRadius, Theme::Surface);
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius, Theme::Divider);
        dot(g, b.X + 18, b.Y + 21, colorForTone(tiles[i].data.tone));
        drawText(g, tiles[i].label, {b.X + 32, b.Y + 14, b.Width - 50, 20}, 12, Gdiplus::FontStyleRegular, Theme::TextMuted,
            Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        drawText(g, tiles[i].data.value, {b.X + 18, b.Y + 36, b.Width - 36, 26}, 18, Gdiplus::FontStyleBold, Theme::TextPrimary);
        drawText(g, tiles[i].data.detail, {b.X + 18, b.Y + 64, b.Width - 36, 20}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextSecondary);
    }
}

void HomeView::drawNews(Gdiplus::Graphics& g, const LauncherViewState& state, const HeroComponent& hero,
                        const ControlVisuals& visuals) const
{
    drawDisplayText(g, L"Novidades", Layout::NewsHeading, 17, Theme::TextPrimary, Gdiplus::StringAlignmentNear,
        Gdiplus::StringAlignmentCenter);
    Buttons::draw(g, {Layout::NewsAll.X + 20, Layout::NewsAll.Y, Layout::NewsAll.Width - 20, Layout::NewsAll.Height}, L"Ver todas",
        visuals(LauncherConstants::NewsAllButtonId), Buttons::Style::Link, std::nullopt, false, 13);
    const float focus[][2] = {{.62F, .4F}, {.15F, .7F}, {.92F, .2F}};
    for (int i = 0; i < LauncherConstants::NewsCardCount; ++i) {
        const auto b = Layout::newsCard(i);
        if (static_cast<std::size_t>(i) >= state.news.size()) {
            drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius, Theme::Divider);
            continue;
        }
        const auto& item = state.news[static_cast<std::size_t>(i)];
        const auto visual = visuals(LauncherConstants::NewsCardFirstId + i);
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
    drawAvatar(g, Layout::Avatar, tone);

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
    const auto modeCard = [&](const Gdiplus::RectF& b, int id, bool selected, Icons::Icon icon, const wchar_t* title,
                              const wchar_t* description) {
        const auto v = visuals(id);
        const auto fill = selected ? Theme::AccentTint : blend(Gdiplus::Color(0, 30, 31, 44), Theme::SurfaceElevated, v.hover);
        fillRoundedRectangle(g, b, 14, fill);
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, 14,
            selected ? Theme::Accent : blend(Theme::OutlineSubtle, Theme::Border, v.hover));
        Icons::draw(g, icon, {b.X + 12, b.Y + 12, 18, 18}, selected ? Theme::AccentLight : Theme::TextMuted, 1.8F);
        drawText(g, title, {b.X + 12, b.Y + 36, b.Width - 20, 18}, 13, Gdiplus::FontStyleBold,
            v.enabled ? Theme::TextPrimary : Theme::TextMuted);
        drawText(g, description, {b.X + 12, b.Y + 56, b.Width - 18, 36}, 11.5F, Gdiplus::FontStyleRegular,
            selected ? Theme::TextSecondary : Theme::TextMuted);
        if (v.focused) drawFocusRing(g, b, 14, Theme::AccentLight);
    };
    modeCard(Layout::OfflineMode, OfflineButtonId, !m.originalMode, Icons::Icon::User, L"Nickname", L"Sem login. Para o servidor do grupo.");
    modeCard(Layout::OriginalMode, OriginalButtonId, m.originalMode, Icons::Icon::Key, L"Conta original", L"Entrar com Microsoft e usar sua skin.");

    const wchar_t* statLabels[] = {L"Última partida", L"Tempo jogado"};
    const std::wstring statValues[] = {m.lastPlayed, m.totalPlayed};
    for (int i = 0; i < 2; ++i) {
        const auto b = Layout::stat(i);
        fillRoundedRectangle(g, b, 12, Theme::SurfaceSunken);
        drawText(g, statLabels[i], {b.X + 12, b.Y + 10, b.Width - 24, 16}, 11.5F, Gdiplus::FontStyleRegular, Theme::TextMuted);
        drawText(g, statValues[i], {b.X + 12, b.Y + 28, b.Width - 24, 22}, 14, Gdiplus::FontStyleBold, Theme::TextPrimary);
    }

    dot(g, Layout::StatusLine.X, Layout::StatusLine.Y + 8, tone);
    drawText(g, m.playing ? L"Jogando agora" : state.headline, {Layout::StatusLine.X + 16, Layout::StatusLine.Y,
        Layout::StatusLine.Width - 16, Layout::StatusLine.Height}, 14, Gdiplus::FontStyleBold, Theme::TextPrimary,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    const std::wstring message = !m.updateWarning.empty() && state.playEnabled ? m.updateWarning : state.operationMessage;
    drawText(g, message, Layout::StatusMessage, 12.5F, Gdiplus::FontStyleRegular,
        isLauncherError(state.operation) ? Theme::Danger : Theme::TextSecondary);
    if (!m.updateWarning.empty() && state.playEnabled)
        Buttons::draw(g, Layout::UpdateLink, L"Baixar launcher", visuals(UpdateDownloadButtonId), Buttons::Style::Link,
            Icons::Icon::Download, false, 12.5F);

    // Split play button: the main action plus a menu of quick actions.
    const auto playVisual = visuals(PlayButtonId);
    const auto menuVisual = visuals(PlayMenuButtonId);
    const bool enabled = state.playEnabled;
    const auto playFill = enabled ? blend(Theme::Accent, Theme::AccentLight, playVisual.pressed ? 1.0F : playVisual.hover) :
        Gdiplus::Color(255, 36, 35, 58);
    fillRoundedRectangle(g, Layout::Play, 14, 6, 6, 14, playFill);
    if (!enabled && m.progress >= 0) {
        const float fraction = std::clamp(m.progress, 0.0F, 100.0F) / 100;
        const auto saved = g.Save();
        auto clip = createRoundedPath(Layout::Play, 14, 6, 6, 14);
        g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
        Gdiplus::SolidBrush fill(Gdiplus::Color(255, 58, 47, 107));
        if (fraction > 0) g.FillRectangle(&fill, Layout::Play.X, Layout::Play.Y, Layout::Play.Width * fraction, Layout::Play.Height);
        else {
            const float x = Layout::Play.X + (std::fmod(m.phase, 1.0F) * 1.4F - .4F) * Layout::Play.Width;
            g.FillRectangle(&fill, x, Layout::Play.Y, Layout::Play.Width * .4F, Layout::Play.Height);
        }
        g.Restore(saved);
    }
    std::wstring label = state.primaryActionLabel.empty() ? L"JOGAR" : state.primaryActionLabel;
    if (!enabled && m.progress > 0 && label.find(L'%') == std::wstring::npos)
        label += L" " + std::to_wstring(static_cast<int>(std::round(m.progress))) + L"%";
    const auto ink = enabled ? Theme::AccentInk : Gdiplus::Color(255, 230, 225, 255);
    const bool showPlayIcon = enabled && (state.operation == LauncherOperation::ReadyToPlay || state.operation == LauncherOperation::NeedsInstallation);
    const float labelWidth = measureText(g, label, 15, Gdiplus::FontStyleBold);
    const float iconWidth = showPlayIcon ? 26.0F : 0.0F;
    const float startX = Layout::Play.X + (Layout::Play.Width - labelWidth - iconWidth) / 2;
    if (showPlayIcon) Icons::draw(g, Icons::Icon::Play, {startX, Layout::Play.Y + 21, 16, 16}, ink);
    drawText(g, label, {startX + iconWidth, Layout::Play.Y, labelWidth + 6, Layout::Play.Height}, 15, Gdiplus::FontStyleBold, ink,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    if (playVisual.focused) drawFocusRing(g, Layout::Play, 12, Theme::AccentInk);
    const auto menuFill = menuVisual.enabled ? blend(Theme::Accent, Theme::AccentLight, menuVisual.pressed ? 1.0F : menuVisual.hover) :
        Gdiplus::Color(255, 36, 35, 58);
    fillRoundedRectangle(g, Layout::PlayMenu, 6, 14, 14, 6, menuFill);
    Icons::draw(g, Icons::Icon::ChevronDown, {Layout::PlayMenu.X + 21, Layout::PlayMenu.Y + 20, 18, 18},
        menuVisual.enabled ? Theme::AccentInk : Theme::TextMuted, 2);
    if (menuVisual.focused) drawFocusRing(g, Layout::PlayMenu, 12, Theme::AccentInk);

    const struct { int id; const wchar_t* label; } quick[] = {
        {QuickFolderButtonId, L"Pasta do jogo"}, {QuickRepairButtonId, L"Reparar"}, {QuickLogsButtonId, L"Logs"}};
    for (int i = 0; i < 3; ++i)
        Buttons::draw(g, Layout::quickAction(i), quick[i].label, visuals(quick[i].id), Buttons::Style::Ghost, std::nullopt, false, 12.5F);
}
