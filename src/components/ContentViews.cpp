#include "components/ContentViews.h"

#include "components/HeroComponent.h"
#include "components/Layout.h"
#include "constants/LauncherConstants.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

using namespace DrawingUtils;

namespace
{
    void card(Gdiplus::Graphics& g, const Gdiplus::RectF& b)
    {
        fillRoundedRectangle(g, b, Theme::CardRadius + 2, Theme::Surface);
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius + 2, Theme::Divider);
    }

    void header(Gdiplus::Graphics& g, std::wstring_view title, std::wstring_view subtitle, bool error = false)
    {
        drawDisplayText(g, title, Layout::PageTitle, 28, Theme::TextPrimary, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        drawText(g, subtitle, Layout::PageSubtitle, 14, Gdiplus::FontStyleRegular, error ? Theme::Danger : Theme::TextSecondary);
    }

    float rise(float entrance, int index, float distance = 20)
    {
        const float t = std::clamp((entrance - .08F * static_cast<float>(index)) / .6F, 0.0F, 1.0F);
        return std::round(distance * (1 - t) * (1 - t) * (1 - t));
    }

    void scrollbar(Gdiplus::Graphics& g, const Gdiplus::RectF& list, float content, float scroll)
    {
        if (content <= list.Height) return;
        const float ratio = list.Height / content;
        const float thumb = std::max(28.0F, list.Height * ratio);
        const float y = list.Y + (list.Height - thumb) * (scroll / (content - list.Height));
        fillPill(g, {list.GetRight() + 8, y, 4, thumb}, Theme::Border);
    }
}

namespace
{
    // Authored scene for the classic version: floating block islands at dusk.
    void classicScene(Gdiplus::Graphics& g, const Gdiplus::RectF& b)
    {
        Gdiplus::LinearGradientBrush sky(Gdiplus::PointF(0, b.Y), Gdiplus::PointF(0, b.GetBottom()),
            Gdiplus::Color(255, 18, 22, 52), Gdiplus::Color(255, 96, 64, 146));
        g.FillRectangle(&sky, b);
        for (int i = 0; i < 34; ++i) {
            Gdiplus::SolidBrush star(Gdiplus::Color(static_cast<BYTE>(110 + i * 41 % 130), 226, 222, 255));
            g.FillRectangle(&star, b.X + static_cast<float>((i * 137 + 23) % 556), b.Y + static_cast<float>((i * 71 + 9) % 118), 3.0F, 3.0F);
        }
        const float cell = 14;
        const auto block = [&](int column, int row, int red, int green, int blue) {
            const int shade = (column * 73 + row * 151) % 5 * 5 - 10;
            const auto channel = [shade](int value) { return static_cast<BYTE>(std::clamp(value + shade, 0, 255)); };
            Gdiplus::SolidBrush brush(Gdiplus::Color(255, channel(red), channel(green), channel(blue)));
            g.FillRectangle(&brush, b.X + static_cast<float>(column) * cell, b.Y + static_cast<float>(row) * cell, cell + .5F, cell + .5F);
        };
        const auto island = [&](int center, int top, int halfWidth, int depth) {
            for (int row = 0; row < depth; ++row) {
                const int half = halfWidth - row * halfWidth / depth;
                for (int column = center - half; column <= center + half; ++column) {
                    if (row == 0) block(column, top, 86, 168, 92);
                    else if (row == 1) block(column, top + row, 128, 92, 66);
                    else block(column, top + row, 92, 94, 116);
                }
            }
        };
        island(20, 9, 13, 6);
        island(5, 11, 5, 4);
        island(35, 7, 6, 5);
        for (int column = 17; column <= 18; ++column) block(column, 8, 196, 62, 70);
        for (int column = 19; column <= 20; ++column) block(column, 8, 226, 226, 236);
        for (int row = 8; row <= 10; ++row) block(5, row, 74, 120, 220);
        for (int row = 4; row <= 6; ++row) block(35, row, 232, 186, 70);
    }

    void chips(Gdiplus::Graphics& g, float x, float y, const std::vector<std::wstring>& labels)
    {
        for (const auto& label : labels) {
            const float width = measureText(g, label, 12, Gdiplus::FontStyleBold) + 24;
            const Gdiplus::RectF pill{x, y, width, 26};
            fillPill(g, pill, Theme::SurfaceElevated);
            drawText(g, label, pill, 12, Gdiplus::FontStyleBold, Theme::TextSecondary, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
            x += width + 8;
        }
    }

    struct VersionCard
    {
        const wchar_t* kicker;
        const wchar_t* title;
        std::vector<std::wstring> tags;
        struct Row { Icons::Icon icon; const wchar_t* text; };
        std::array<Row, 4> rows;
    };
}

void VersionsView::draw(Gdiplus::Graphics& g, const VersionsModel& m, const HeroComponent& hero, const ControlVisuals& visuals) const
{
    using LauncherConstants::VersionEternalButtonId;
    using LauncherConstants::VersionVanillaButtonId;
    header(g, L"Versões", L"Escolha o que você quer jogar. O botão Jogar da tela inicial abre a versão selecionada.");
    const float radius = Theme::CardRadius + 2;
    const float artHeight = 250;
    const VersionCard cards[] = {
        {L"MODPACK DO GRUPO", L"Eternal Realms",
         {L"Minecraft 1.20.1", L"Forge 47.4.10", m.serverMods >= 0 ? std::to_wstring(m.serverMods) + L" mods" : std::wstring(L"Mods do grupo")},
         {{{Icons::Icon::Server, L"Feito para o servidor do grupo"},
           {Icons::Icon::Clock, L"Seu tempo conta no ranking"},
           {Icons::Icon::Download, L"Mods sincronizados a cada Jogar"},
           {Icons::Icon::Folder, L"Aceita os seus próprios mods"}}}},
        {L"VERSÃO CLÁSSICA", L"Minecraft 1.8.9",
         {L"Sem mods", L"Java 8", m.vanillaInstalled ? std::wstring(L"Instalada") : std::wstring(L"Baixa no primeiro jogo")},
         {{{Icons::Icon::Play, L"Boa para Bedwars, SkyWars e outros minigames"},
           {Icons::Icon::Layers, L"Pasta e Java próprios, sem mexer no modpack"},
           {Icons::Icon::Clock, L"Este tempo não conta no ranking"},
           {Icons::Icon::Key, L"Hypixel e servidores premium pedem Conta original"}}}}};
    for (int i = 0; i < 2; ++i) {
        const bool selected = (i == 1) == m.vanillaSelected;
        const float offset = rise(m.entrance, i);
        auto b = Layout::versionCard(i);
        b.Y += offset;
        fillRoundedRectangle(g, b, radius, Theme::Surface);
        {
            const Gdiplus::RectF art{b.X, b.Y, b.Width, artHeight};
            const auto saved = g.Save();
            auto clip = createRoundedPath(b, radius);
            g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
            if (i == 0) hero.drawThumbnail(g, art, 3, .62F, .42F);
            else classicScene(g, art);
            // The artwork dissolves into the card so the copy below reads as part of it.
            const Gdiplus::Color surface = Theme::Surface;
            Gdiplus::LinearGradientBrush fade(Gdiplus::PointF(0, art.GetBottom() - 96), Gdiplus::PointF(0, art.GetBottom() + 1),
                Gdiplus::Color(0, surface.GetR(), surface.GetG(), surface.GetB()), surface);
            g.FillRectangle(&fade, Gdiplus::RectF(art.X, art.GetBottom() - 95, art.Width, 95));
            g.Restore(saved);
        }
        if (selected) drawRoundedRectangle(g, {b.X + 1, b.Y + 1, b.Width - 2, b.Height - 2}, radius, Theme::Accent, 2);
        else drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, radius, Theme::Divider);

        const auto& card = cards[i];
        const float top = b.Y + artHeight - 22;
        const float left = b.X + 28, width = b.Width - 56;
        drawText(g, card.kicker, {left, top, width, 18}, 11.5F, Gdiplus::FontStyleBold, Theme::AccentLight);
        drawDisplayText(g, card.title, {left, top + 20, width - 130, 40}, 26, Theme::TextPrimary, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        if (selected) {
            const Gdiplus::RectF pill{b.GetRight() - 28 - 122, top + 27, 122, 28};
            fillPill(g, pill, Theme::AccentTint);
            Icons::draw(g, Icons::Icon::Check, {pill.X + 13, pill.Y + 7, 14, 14}, Theme::AccentLight, 2);
            drawText(g, L"Selecionada", {pill.X + 32, pill.Y, pill.Width - 36, pill.Height}, 12.5F, Gdiplus::FontStyleBold, Theme::AccentLight,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        }
        chips(g, left, top + 68, card.tags);
        for (std::size_t row = 0; row < card.rows.size(); ++row) {
            const float y = top + 108 + static_cast<float>(row) * 31;
            Icons::draw(g, card.rows[row].icon, {left, y + 6, 18, 18}, Theme::AccentLight, 1.7F);
            drawText(g, card.rows[row].text, {left + 30, y, width - 30, 30}, 14, Gdiplus::FontStyleRegular, Theme::TextSecondary,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        }

        auto action = Layout::versionAction(i);
        action.Y += offset;
        const auto visual = visuals(i == 0 ? VersionEternalButtonId : VersionVanillaButtonId);
        if (selected) Buttons::draw(g, action, L"JOGAR", visual, Buttons::Style::Primary, Icons::Icon::Play, false, 15);
        else Buttons::draw(g, action, L"Usar esta versão", visual, Buttons::Style::Secondary, std::nullopt, false, 14);
    }
}

void NewsView::draw(Gdiplus::Graphics& g, const NewsModel& m, const ControlVisuals& visuals) const
{
    const auto count = m.items ? m.items->size() : 0;
    std::wstring subtitle = m.loading ? L"Carregando avisos do servidor..." : !m.error.empty() ? m.error :
        L"Avisos do servidor e novidades do launcher.";
    header(g, L"Notícias", subtitle, !m.error.empty());
    if (count == 0) {
        const auto b = Layout::newsEntry(0);
        card(g, b);
        drawText(g, L"Nenhuma notícia por enquanto", {b.X + 28, b.Y + 34, b.Width - 56, 26}, 17, Gdiplus::FontStyleBold, Theme::TextPrimary);
        drawText(g, L"Quando o servidor publicar avisos, eles aparecem aqui e na tela inicial.", {b.X + 28, b.Y + 66, b.Width - 56, 40},
            13.5F, Gdiplus::FontStyleRegular, Theme::TextSecondary);
        return;
    }
    for (std::size_t slot = 0; slot < Visible && m.first + slot < count; ++slot) {
        const auto& item = (*m.items)[m.first + slot];
        auto b = Layout::newsEntry(static_cast<int>(slot));
        b.Y += rise(m.entrance, static_cast<int>(slot));
        card(g, b);
        const float categoryWidth = measureText(g, item.category, 11, Gdiplus::FontStyleBold) + 20;
        const Gdiplus::RectF pill{b.X + 24, b.Y + 22, categoryWidth, 22};
        fillPill(g, pill, Theme::AccentTint);
        drawText(g, item.category, pill, 11, Gdiplus::FontStyleBold, Theme::AccentLight, Gdiplus::StringAlignmentCenter,
            Gdiplus::StringAlignmentCenter);
        float dateX = pill.GetRight() + 12;
        if (item.unread) {
            // Unread on arrival: a solid accent tag and an accent edge on the card.
            const Gdiplus::RectF fresh{pill.GetRight() + 8, pill.Y, 54, 22};
            fillPill(g, fresh, Theme::Accent);
            drawText(g, L"NOVO", fresh, 11, Gdiplus::FontStyleBold, Theme::AccentInk, Gdiplus::StringAlignmentCenter,
                Gdiplus::StringAlignmentCenter);
            fillPill(g, {b.X, b.Y + 24, 3, b.Height - 48}, Theme::Accent);
            dateX = fresh.GetRight() + 12;
        }
        drawText(g, item.date, {dateX, pill.Y, 260, 22}, 12, Gdiplus::FontStyleRegular, Theme::TextMuted,
            Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        drawDisplayText(g, item.title, {b.X + 24, b.Y + 52, b.Width - 200, 28}, 18, Theme::TextPrimary);
        drawText(g, item.description, {b.X + 24, b.Y + 84, b.Width - 200, 44}, 13, Gdiplus::FontStyleRegular, Theme::TextSecondary);
        auto link = Layout::newsLink(static_cast<int>(slot));
        link.Y += rise(m.entrance, static_cast<int>(slot));
        if (!item.url.empty())
            Buttons::draw(g, link, L"Abrir", visuals(LauncherConstants::NewsLinkFirstId + static_cast<int>(slot)),
                Buttons::Style::Ghost, Icons::Icon::External);
    }
    if (count > Visible) {
        const std::wstring position = std::to_wstring(m.first + 1) + L"–" + std::to_wstring(std::min(count, m.first + Visible)) +
            L" de " + std::to_wstring(count) + L" · role para ver mais";
        drawText(g, position, {Layout::PageSubtitle.X + 600, Layout::PageSubtitle.Y, 556, 22}, 12.5F, Gdiplus::FontStyleRegular,
            Theme::TextMuted, Gdiplus::StringAlignmentFar);
    }
}

Gdiplus::RectF ModsView::serverList() { return {Layout::ModsServer.X + 24, Layout::ModsServer.Y + 88, Layout::ModsServer.Width - 56, Layout::ModsServer.Height - 104}; }
Gdiplus::RectF ModsView::userList() { return {Layout::ModsUser.X + 24, Layout::ModsUser.Y + 88, Layout::ModsUser.Width - 56, Layout::ModsUser.Height - 104}; }

float ModsView::maximumServerScroll(const ModsModel& m)
{
    const float content = m.inventory ? static_cast<float>(m.inventory->server.size()) * ServerRowHeight : 0;
    return std::max(0.0F, content - serverList().Height);
}

float ModsView::maximumUserScroll(const ModsModel& m)
{
    const float content = m.inventory ? static_cast<float>(m.inventory->user.size()) * UserRowHeight : 0;
    return std::max(0.0F, content - userList().Height);
}

ModsView::Hit ModsView::hitTest(const ModsModel& m, float x, float y)
{
    Hit hit;
    if (!m.inventory) return hit;
    const auto list = userList();
    if (!contains(list, x, y)) return hit;
    const int row = static_cast<int>(std::floor((y - list.Y + m.userScroll) / UserRowHeight));
    if (row < 0 || static_cast<std::size_t>(row) >= m.inventory->user.size()) return hit;
    hit.row = row;
    const float top = list.Y + static_cast<float>(row) * UserRowHeight - m.userScroll;
    const Gdiplus::RectF toggle{list.GetRight() - 104, top + 17, 44, 26};
    const Gdiplus::RectF remove{list.GetRight() - 44, top + 12, 36, 36};
    if (contains(toggle, x, y)) hit.action = Action::Toggle;
    else if (contains(remove, x, y)) hit.action = Action::Remove;
    return hit;
}

void ModsView::draw(Gdiplus::Graphics& g, const ModsModel& m, const ControlVisuals& visuals) const
{
    const auto* inventory = m.inventory;
    const std::size_t server = inventory ? inventory->server.size() : 0;
    const std::size_t user = inventory ? inventory->user.size() : 0;
    std::wstring subtitle = !m.message.empty() ? m.message :
        inventory && inventory->manifestKnown ? std::to_wstring(server) + L" do servidor · " + std::to_wstring(user) + (user == 1 ? L" seu" : L" seus") :
        L"Aguardando a lista de mods do servidor.";
    header(g, L"Mods", subtitle, m.messageError);
    Buttons::drawIcon(g, Layout::ModsRefresh, Icons::Icon::Refresh, visuals(LauncherConstants::ModsRefreshButtonId), false, 18);
    Buttons::draw(g, Layout::ModsFolder, L"Abrir pasta", visuals(LauncherConstants::ModsFolderButtonId), Buttons::Style::Ghost, Icons::Icon::Folder);
    Buttons::draw(g, Layout::ModsAdd, L"Adicionar mod", visuals(LauncherConstants::ModsAddButtonId), Buttons::Style::Primary, Icons::Icon::Plus);

    // Server mods: read-only, kept up to date by the launcher.
    card(g, Layout::ModsServer);
    drawText(g, L"Do servidor", {Layout::ModsServer.X + 24, Layout::ModsServer.Y + 22, 300, 24}, 16, Gdiplus::FontStyleBold, Theme::TextPrimary);
    drawText(g, L"Instalados e atualizados pelo launcher a cada Jogar.", {Layout::ModsServer.X + 24, Layout::ModsServer.Y + 48, 500, 20},
        12.5F, Gdiplus::FontStyleRegular, Theme::TextMuted);
    const auto serverBounds = serverList();
    {
        const auto saved = g.Save();
        g.SetClip(serverBounds, Gdiplus::CombineModeIntersect);
        if (server == 0)
            drawText(g, inventory && inventory->manifestKnown ? L"O servidor não lista nenhum mod." : L"A lista aparece quando o servidor responder.",
                {serverBounds.X, serverBounds.Y + 8, serverBounds.Width, 40}, 13, Gdiplus::FontStyleRegular, Theme::TextMuted);
        for (std::size_t i = 0; i < server; ++i) {
            const float top = serverBounds.Y + static_cast<float>(i) * ServerRowHeight - m.serverScroll;
            if (top + ServerRowHeight < serverBounds.Y || top > serverBounds.GetBottom()) continue;
            const auto& mod = inventory->server[i];
            Gdiplus::SolidBrush state(mod.installed ? Theme::Success : Theme::TextMuted);
            g.FillEllipse(&state, serverBounds.X, top + 16, 7.0F, 7.0F);
            drawText(g, mod.name, {serverBounds.X + 18, top, serverBounds.Width - 130, ServerRowHeight}, 13, Gdiplus::FontStyleRegular,
                Theme::TextPrimary, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
            drawText(g, mod.installed ? ModInventory::formatSize(mod.size) : L"Baixa ao jogar", {serverBounds.GetRight() - 110, top, 110, ServerRowHeight},
                12, Gdiplus::FontStyleRegular, Theme::TextMuted, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter);
            if (i + 1 < server) {
                Gdiplus::Pen line(Theme::Divider);
                g.DrawLine(&line, serverBounds.X, top + ServerRowHeight - .5F, serverBounds.GetRight(), top + ServerRowHeight - .5F);
            }
        }
        g.Restore(saved);
    }
    scrollbar(g, serverBounds, static_cast<float>(server) * ServerRowHeight, m.serverScroll);

    // Player mods: the launcher never deletes these.
    card(g, Layout::ModsUser);
    drawText(g, L"Seus mods", {Layout::ModsUser.X + 24, Layout::ModsUser.Y + 22, 300, 24}, 16, Gdiplus::FontStyleBold, Theme::TextPrimary);
    drawText(g, L"Você decide: ativar, desativar ou remover. O launcher nunca apaga.",
        {Layout::ModsUser.X + 24, Layout::ModsUser.Y + 48, 520, 20}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextMuted);
    const auto userBounds = userList();
    {
        const auto saved = g.Save();
        g.SetClip(userBounds, Gdiplus::CombineModeIntersect);
        if (user == 0) {
            Icons::draw(g, Icons::Icon::Mods, {userBounds.X + userBounds.Width / 2 - 18, userBounds.Y + 120, 36, 36}, Theme::TextMuted, 1.6F);
            drawText(g, L"Nenhum mod seu ainda", {userBounds.X, userBounds.Y + 168, userBounds.Width, 24}, 15, Gdiplus::FontStyleBold,
                Theme::TextPrimary, Gdiplus::StringAlignmentCenter);
            drawText(g, L"Use Adicionar mod ou coloque arquivos .jar na pasta de mods.\nEles continuam lá mesmo quando o servidor muda.",
                {userBounds.X + 40, userBounds.Y + 196, userBounds.Width - 80, 44}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextSecondary,
                Gdiplus::StringAlignmentCenter);
        }
        for (std::size_t i = 0; i < user; ++i) {
            const float top = userBounds.Y + static_cast<float>(i) * UserRowHeight - m.userScroll;
            if (top + UserRowHeight < userBounds.Y || top > userBounds.GetBottom()) continue;
            const auto& mod = inventory->user[i];
            const bool hovered = m.hoveredRow == static_cast<int>(i);
            if (hovered) fillRoundedRectangle(g, {userBounds.X - 8, top + 2, userBounds.Width + 8, UserRowHeight - 4}, 12, Theme::SurfaceSunken);
            drawText(g, mod.name, {userBounds.X, top + 10, userBounds.Width - 130, 22}, 13.5F, Gdiplus::FontStyleBold,
                mod.enabled ? Theme::TextPrimary : Theme::TextMuted);
            drawText(g, ModInventory::formatSize(mod.size) + (mod.enabled ? L" · Ativo" : L" · Desativado"),
                {userBounds.X, top + 32, userBounds.Width - 130, 18}, 12, Gdiplus::FontStyleRegular, Theme::TextMuted);
            ControlVisual toggle{hovered && m.hoveredAction == 0 ? 1.0F : 0.0F, false, false, !m.locked};
            float knob = mod.enabled ? 1.0F : 0.0F;
            if (m.knobs) if (const auto found = m.knobs->find(mod.name); found != m.knobs->end()) knob = found->second;
            Buttons::drawSwitch(g, {userBounds.GetRight() - 104, top + 17, 44, 26}, knob, toggle);
            ControlVisual remove{hovered && m.hoveredAction == 1 ? 1.0F : 0.0F, false, false, !m.locked};
            const Gdiplus::Color removeHover(255, 52, 30, 40);
            Buttons::drawIcon(g, {userBounds.GetRight() - 44, top + 12, 36, 36}, Icons::Icon::Trash, remove, false, 17, &removeHover);
        }
        g.Restore(saved);
    }
    scrollbar(g, userBounds, static_cast<float>(user) * UserRowHeight, m.userScroll);
}

std::size_t ScreenshotsView::rowCount(std::size_t entries)
{
    return (entries + Layout::ScreenshotColumns - 1) / Layout::ScreenshotColumns;
}

std::size_t ScreenshotsView::lastFirstRow(std::size_t entries)
{
    const auto rows = rowCount(entries);
    return rows > static_cast<std::size_t>(Layout::ScreenshotRows) ? rows - Layout::ScreenshotRows : 0;
}

void ScreenshotsView::draw(Gdiplus::Graphics& g, const ScreenshotsModel& m, const ControlVisuals& visuals) const
{
    if (m.viewer >= 0 && static_cast<std::size_t>(m.viewer) < m.cards.size()) drawViewer(g, m, visuals);
    else drawGrid(g, m, visuals);
}

namespace
{
    // "Minhas" / "Do grupo": one pill slides under the selected half.
    void screenshotTabs(Gdiplus::Graphics& g, bool group, const ControlVisuals& visuals)
    {
        using namespace LauncherConstants;
        const auto& box = Layout::ScreenshotsTabs;
        fillRoundedRectangle(g, box, 14, Theme::SurfaceSunken);
        drawRoundedRectangle(g, {box.X + .5F, box.Y + .5F, box.Width - 1, box.Height - 1}, 14, Theme::OutlineSubtle);
        const struct { int id; const wchar_t* label; Icons::Icon icon; bool selected; } tabs[] = {
            {ScreenshotsMineTabId, L"Minhas capturas", Icons::Icon::Image, !group},
            {ScreenshotsGroupTabId, L"Galeria do grupo", Icons::Icon::Users, group}};
        for (int i = 0; i < 2; ++i) {
            const auto b = Layout::screenshotsTab(i);
            const auto v = visuals(tabs[i].id);
            if (tabs[i].selected) {
                fillRoundedRectangle(g, b, 11, Theme::AccentTint);
                drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, 11, Theme::AccentOutline);
            } else if (v.hover > .01F) {
                fillRoundedRectangle(g, b, 11, blend(Gdiplus::Color(0, 30, 31, 44), Theme::SurfaceElevated, v.hover));
            }
            const auto ink = tabs[i].selected ? Theme::AccentLight : blend(Theme::TextMuted, Theme::TextPrimary, v.hover);
            const float width = measureText(g, tabs[i].label, 12.5F, Gdiplus::FontStyleBold) + 24;
            const float x = b.X + (b.Width - width) / 2;
            Icons::draw(g, tabs[i].icon, {x, b.Y + 8, 16, 16}, ink, 1.8F);
            drawTextLine(g, tabs[i].label, {x + 24, b.Y, width, b.Height}, 12.5F, Gdiplus::FontStyleBold,
                tabs[i].selected ? Theme::TextPrimary : ink);
            if (v.focused) drawFocusRing(g, b, 11, Theme::AccentLight);
        }
    }

    void placeholderShimmer(Gdiplus::Graphics& g, const Gdiplus::RectF& image, float time, std::size_t slot)
    {
        Gdiplus::SolidBrush placeholder(Theme::SurfaceSunken);
        g.FillRectangle(&placeholder, image);
        Icons::draw(g, Icons::Icon::Image, {image.X + image.Width / 2 - 14, image.Y + image.Height / 2 - 14, 28, 28}, Theme::Border, 1.6F);
        // Still loading: a band of light crosses the placeholder.
        const float t = std::fmod(time / 1.4F + static_cast<float>(slot) * .07F, 1.0F);
        const float center = image.X - 80 + (image.Width + 160) * t;
        Gdiplus::LinearGradientBrush band(Gdiplus::PointF(center - 60, 0), Gdiplus::PointF(center + 60, 0),
            Gdiplus::Color(0, 255, 255, 255), Gdiplus::Color(0, 255, 255, 255));
        const Gdiplus::Color colors[] = {Gdiplus::Color(0, 255, 255, 255), Gdiplus::Color(14, 255, 255, 255), Gdiplus::Color(0, 255, 255, 255)};
        const Gdiplus::REAL positions[] = {0, .5F, 1};
        band.SetInterpolationColors(colors, positions, 3);
        g.FillRectangle(&band, center - 60, image.Y, 120.0F, image.Height);
    }
}

void ScreenshotsView::drawGrid(Gdiplus::Graphics& g, const ScreenshotsModel& m, const ControlVisuals& visuals) const
{
    using namespace LauncherConstants;
    const auto count = m.cards.size();
    std::wstring subtitle;
    if (!m.message.empty()) subtitle = m.message;
    else if (m.group) subtitle = m.loading && count == 0 ? L"Carregando as capturas do grupo..." :
        m.unavailable ? L"A galeria do grupo não respondeu agora." :
        count == 0 ? L"As capturas que o grupo compartilha aparecem aqui." :
        std::to_wstring(count) + (count == 1 ? L" captura compartilhada" : L" capturas compartilhadas") + L" pelo grupo";
    else subtitle = count == 0 ? std::wstring(L"Suas capturas de tela do jogo, num lugar só.") :
        std::to_wstring(count) + (count == 1 ? L" captura" : L" capturas") + L" · aperte F2 no jogo para tirar mais";
    header(g, L"Capturas", subtitle, m.messageError || (m.group && m.unavailable && m.message.empty()));
    screenshotTabs(g, m.group, visuals);
    Buttons::drawIcon(g, Layout::ScreenshotsRefresh, Icons::Icon::Refresh, visuals(ScreenshotsRefreshButtonId), false, 18);
    if (!m.group)
        Buttons::draw(g, Layout::ScreenshotsFolder, L"Abrir pasta", visuals(ScreenshotsFolderButtonId), Buttons::Style::Ghost, Icons::Icon::Folder);

    if (count == 0) {
        const Gdiplus::RectF b{100, 168, 1156, 608};
        auto box = b;
        box.Y += rise(m.entrance, 0);
        card(g, box);
        const float cx = box.X + box.Width / 2, top = box.Y + 190;
        fillRoundedRectangle(g, {cx - 36, top, 72, 72}, 20, Theme::AccentTint);
        if (m.group && m.loading) {
            // Loading: the emblem's ring turns.
            Gdiplus::Pen arc(Theme::AccentLight, 2.4F);
            arc.SetStartCap(Gdiplus::LineCapRound);
            arc.SetEndCap(Gdiplus::LineCapRound);
            g.DrawArc(&arc, cx - 18, top + 18, 36.0F, 36.0F, std::fmod(m.time * 360, 360.0F), 250);
        } else {
            Icons::draw(g, m.group ? Icons::Icon::Users : Icons::Icon::Image, {cx - 18, top + 18, 36, 36}, Theme::AccentLight, 1.8F);
        }
        const wchar_t* title = m.group ? (m.loading ? L"Buscando a galeria" : m.unavailable ? L"Galeria indisponível" : L"A galeria está vazia")
                                       : L"Nenhuma captura ainda";
        const wchar_t* text = m.group
            ? (m.loading ? L"Só um instante." : m.unavailable ? L"O servidor da galeria não respondeu. Tente atualizar daqui a pouco."
                         : L"Abra uma das suas capturas e use Compartilhar.\nEla aparece aqui para todo mundo do grupo, com o seu nickname.")
            : L"Dentro do jogo, aperte F2 para tirar uma captura de tela.\nEla aparece aqui, pronta para ver, copiar e mandar para o grupo.";
        drawDisplayText(g, title, {box.X, top + 94, box.Width, 32}, 20, Theme::TextPrimary, Gdiplus::StringAlignmentCenter,
            Gdiplus::StringAlignmentCenter);
        drawText(g, text, {box.X + 200, top + 134, box.Width - 400, 48}, 13.5F, Gdiplus::FontStyleRegular, Theme::TextSecondary,
            Gdiplus::StringAlignmentCenter);
        return;
    }

    const std::size_t first = m.firstRow * Layout::ScreenshotColumns;
    for (std::size_t slot = 0; slot < Visible && first + slot < count; ++slot) {
        const auto& item = m.cards[first + slot];
        const int column = static_cast<int>(slot) % Layout::ScreenshotColumns;
        const int row = static_cast<int>(slot) / Layout::ScreenshotColumns;
        auto b = Layout::screenshotCard(static_cast<int>(slot));
        b.Y += rise(m.entrance, row + column / 2);
        const auto visual = visuals(ScreenshotCardFirstId + static_cast<int>(slot));
        // Hover lifts the card over a soft shadow, like the news cards on Home.
        if (visual.hover > .01F) {
            for (int layer = 3; layer >= 1; --layer) {
                const float spread = static_cast<float>(layer) * 3;
                fillRoundedRectangle(g, {b.X - spread + 4, b.Y + 6 + spread, b.Width + 2 * spread - 8, b.Height - 4},
                    Theme::CardRadius + spread, Gdiplus::Color(static_cast<BYTE>(22 * visual.hover), 0, 0, 0));
            }
        }
        b.Y -= std::round(4 * visual.hover);
        fillRoundedRectangle(g, b, Theme::CardRadius, blend(Theme::Surface, Theme::SurfaceElevated, visual.hover * .6F));
        const Gdiplus::RectF image{b.X, b.Y, b.Width, Layout::ScreenshotImageHeight};
        {
            const auto saved = g.Save();
            auto clip = createRoundedPath(b, Theme::CardRadius);
            g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
            const Gdiplus::Bitmap* thumbnail = nullptr;
            bool known = false;
            if (m.thumbnails)
                if (const auto found = m.thumbnails->find(item.key); found != m.thumbnails->end()) { thumbnail = found->second.get(); known = true; }
            if (thumbnail) {
                g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
                g.DrawImage(const_cast<Gdiplus::Bitmap*>(thumbnail), image);
                // Hover brightens the picture a touch.
                if (visual.hover > .01F) {
                    Gdiplus::SolidBrush light(Gdiplus::Color(static_cast<BYTE>(18 * visual.hover), 255, 255, 255));
                    g.FillRectangle(&light, image);
                }
            } else if (known) {
                // Decoding failed: a still placeholder, no shimmer.
                Gdiplus::SolidBrush placeholder(Theme::SurfaceSunken);
                g.FillRectangle(&placeholder, image);
                Icons::draw(g, Icons::Icon::Image, {image.X + image.Width / 2 - 14, image.Y + image.Height / 2 - 14, 28, 28}, Theme::Border, 1.6F);
            } else {
                placeholderShimmer(g, image, m.time, slot);
            }
            g.Restore(saved);
        }
        float chipX = image.X + 10;
        if (!item.badge.empty()) {
            const float width = measureText(g, item.badge, 11, Gdiplus::FontStyleBold) + 20;
            const Gdiplus::RectF chip{chipX, image.Y + 10, width, 22};
            fillPill(g, chip, Theme::Glass);
            drawText(g, item.badge, chip, 11, Gdiplus::FontStyleBold, Theme::TextPrimary, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
            chipX = chip.GetRight() + 6;
        }
        if (item.shared) {
            // Already in the group gallery.
            const Gdiplus::RectF chip{image.GetRight() - 34, image.Y + 10, 24, 24};
            fillPill(g, chip, Theme::Accent);
            Icons::draw(g, Icons::Icon::Users, {chip.X + 5, chip.Y + 5, 14, 14}, Theme::AccentInk, 1.8F);
        }
        const float footer = b.GetBottom() - image.GetBottom() - 8;
        const float detailWidth = item.detail.empty() ? 0.0F : std::min(120.0F, measureText(g, item.detail, 12, Gdiplus::FontStyleRegular) + 6);
        drawTextLine(g, item.title, {b.X + 14, image.GetBottom() + 4, b.Width - 28 - detailWidth - 6, footer}, 12.5F, Gdiplus::FontStyleBold,
            Theme::TextPrimary);
        if (!item.detail.empty())
            drawTextLine(g, item.detail, {b.GetRight() - 14 - detailWidth, image.GetBottom() + 4, detailWidth, footer}, 12,
                Gdiplus::FontStyleRegular, Theme::TextMuted, Gdiplus::StringAlignmentFar);
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, Theme::CardRadius,
            blend(Theme::Divider, Theme::AccentOutline, visual.hover));
        if (visual.focused) drawFocusRing(g, b, Theme::CardRadius, Theme::AccentLight);
    }
    const auto rows = rowCount(count);
    if (rows > static_cast<std::size_t>(Layout::ScreenshotRows)) {
        const Gdiplus::RectF list{100, 168, 1156, 608};
        scrollbar(g, list, static_cast<float>(rows) * 206 - 14, static_cast<float>(m.firstRow) * 206 *
            (static_cast<float>(rows) * 206 - 14 - list.Height) / std::max(1.0F, static_cast<float>(lastFirstRow(count)) * 206));
        const std::wstring position = std::to_wstring(first + 1) + L"–" + std::to_wstring(std::min(count, first + Visible)) +
            L" de " + std::to_wstring(count) + L" · role para ver mais";
        drawText(g, position, {Layout::PageSubtitle.X + 600, Layout::PageSubtitle.Y, 556, 22}, 12.5F, Gdiplus::FontStyleRegular,
            Theme::TextMuted, Gdiplus::StringAlignmentFar);
    }
}

void ScreenshotsView::drawViewer(Gdiplus::Graphics& g, const ScreenshotsModel& m, const ControlVisuals& visuals) const
{
    using namespace LauncherConstants;
    const auto count = m.cards.size();
    const auto index = static_cast<std::size_t>(m.viewer);
    const auto& item = m.cards[index];

    Buttons::draw(g, Layout::ViewerBack, L"Voltar", visuals(ViewerBackButtonId), Buttons::Style::Ghost, Icons::Icon::ChevronLeft);
    const auto& title = Layout::ViewerTitle;
    drawDisplayText(g, m.viewerTitle, {title.X, title.Y, title.Width, 28}, 19, Theme::TextPrimary, Gdiplus::StringAlignmentNear,
        Gdiplus::StringAlignmentCenter);
    const bool confirmation = !m.message.empty() && !m.messageError;
    const std::wstring detail = !m.message.empty() ? m.message : m.viewerDetail;
    if (confirmation) Icons::draw(g, Icons::Icon::Check, {title.X, title.Y + 33, 14, 14}, Theme::Success, 1.8F);
    drawTextLine(g, detail, {title.X + (confirmation ? 20.0F : 0.0F), title.Y + 30, title.Width - (confirmation ? 20.0F : 0.0F), 20}, 12.5F,
        Gdiplus::FontStyleRegular, m.messageError ? Theme::Danger : confirmation ? Theme::TextPrimary : Theme::TextMuted);

    const Gdiplus::Color removeHover(255, 52, 30, 40);
    if (m.group) {
        Buttons::draw(g, Layout::ViewerGroupCopy, L"Copiar", visuals(ViewerCopyButtonId), Buttons::Style::Secondary, Icons::Icon::Copy);
        if (m.viewerMine)
            Buttons::draw(g, Layout::ViewerGroupAction, L"Remover", visuals(ViewerDeleteButtonId), Buttons::Style::Danger, Icons::Icon::Trash);
        else
            Buttons::draw(g, Layout::ViewerGroupAction, L"Denunciar", visuals(ViewerReportButtonId), Buttons::Style::Ghost, Icons::Icon::Flag);
    } else {
        // Sharing is the highlight of a local picture; once shared it says so.
        auto share = visuals(ViewerShareButtonId);
        if (m.viewerShared || m.sharing) share.enabled = false;
        if (m.viewerShared) {
            fillRoundedRectangle(g, Layout::ViewerShare, Theme::ControlRadius, Theme::AccentTint);
            drawRoundedRectangle(g, {Layout::ViewerShare.X + .5F, Layout::ViewerShare.Y + .5F, Layout::ViewerShare.Width - 1,
                Layout::ViewerShare.Height - 1}, Theme::ControlRadius, Theme::AccentOutline);
            Icons::draw(g, Icons::Icon::Check, {Layout::ViewerShare.X + 14, Layout::ViewerShare.Y + 14, 16, 16}, Theme::AccentLight, 2);
            drawTextLine(g, L"No grupo", {Layout::ViewerShare.X + 38, Layout::ViewerShare.Y, Layout::ViewerShare.Width - 42, Layout::ViewerShare.Height},
                13, Gdiplus::FontStyleBold, Theme::AccentLight);
        } else {
            Buttons::draw(g, Layout::ViewerShare, m.sharing ? L"Enviando..." : L"Compartilhar", share, Buttons::Style::Primary,
                Icons::Icon::Share, false, 12.5F);
        }
        Buttons::draw(g, Layout::ViewerCopy, L"Copiar", visuals(ViewerCopyButtonId), Buttons::Style::Secondary, Icons::Icon::Copy);
        Buttons::draw(g, Layout::ViewerFolder, L"Mostrar na pasta", visuals(ViewerFolderButtonId), Buttons::Style::Ghost, Icons::Icon::Folder, false, 12.5F);
        Buttons::draw(g, Layout::ViewerOpen, L"Abrir", visuals(ViewerOpenButtonId), Buttons::Style::Ghost, Icons::Icon::External);
        Buttons::drawIcon(g, Layout::ViewerDelete, Icons::Icon::Trash, visuals(ViewerDeleteButtonId), false, 18, &removeHover);
    }

    // The picture fits the stage whole, on a dark plate.
    const auto& stage = Layout::ViewerImage;
    fillRoundedRectangle(g, stage, Theme::PanelRadius, Gdiplus::Color(255, 8, 9, 13));
    const Gdiplus::Bitmap* picture = m.viewerImage;
    bool preview = false;
    if (!picture && m.thumbnails) {
        // While the full picture loads, the grid thumbnail stands in.
        if (const auto found = m.thumbnails->find(item.key); found != m.thumbnails->end() && found->second) {
            picture = found->second.get();
            preview = true;
        }
    }
    if (picture) {
        auto* bitmap = const_cast<Gdiplus::Bitmap*>(picture);
        float pictureWidth = static_cast<float>(bitmap->GetWidth()), pictureHeight = static_cast<float>(bitmap->GetHeight());
        if (preview && m.viewerWidth > 0 && m.viewerHeight > 0) {
            pictureWidth = static_cast<float>(m.viewerWidth);
            pictureHeight = static_cast<float>(m.viewerHeight);
        }
        const float scale = std::min(stage.Width / pictureWidth, stage.Height / pictureHeight);
        const Gdiplus::RectF placed{stage.X + (stage.Width - pictureWidth * scale) / 2, stage.Y + (stage.Height - pictureHeight * scale) / 2,
            pictureWidth * scale, pictureHeight * scale};
        const auto saved = g.Save();
        auto clip = createRoundedPath(stage, Theme::PanelRadius);
        g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
        if (preview) {
            // A thumbnail is a center crop; show the matching part of the stage.
            const float cropScale = std::max(placed.Width / static_cast<float>(bitmap->GetWidth()), placed.Height / static_cast<float>(bitmap->GetHeight()));
            const float w = static_cast<float>(bitmap->GetWidth()) * cropScale, h = static_cast<float>(bitmap->GetHeight()) * cropScale;
            g.SetClip(placed, Gdiplus::CombineModeIntersect);
            g.DrawImage(bitmap, Gdiplus::RectF(placed.X + (placed.Width - w) / 2, placed.Y + (placed.Height - h) / 2, w, h));
        } else {
            g.DrawImage(bitmap, placed);
        }
        g.Restore(saved);
    } else {
        Icons::draw(g, Icons::Icon::Image, {stage.X + stage.Width / 2 - 20, stage.Y + stage.Height / 2 - 34, 40, 40}, Theme::Border, 1.6F);
        drawText(g, L"Carregando captura...", {stage.X, stage.Y + stage.Height / 2 + 14, stage.Width, 22}, 13, Gdiplus::FontStyleRegular,
            Theme::TextMuted, Gdiplus::StringAlignmentCenter);
    }
    drawRoundedRectangle(g, {stage.X + .5F, stage.Y + .5F, stage.Width - 1, stage.Height - 1}, Theme::PanelRadius, Theme::Divider);

    // Previous / next: round glass buttons floating over the picture's edges.
    const struct { int id; const Gdiplus::RectF* bounds; Icons::Icon icon; bool available; } arrows[] = {
        {ViewerPreviousButtonId, &Layout::ViewerPrevious, Icons::Icon::ChevronLeft, index > 0},
        {ViewerNextButtonId, &Layout::ViewerNext, Icons::Icon::ChevronRight, index + 1 < count}};
    for (const auto& arrow : arrows) {
        if (!arrow.available) continue;
        const auto& b = *arrow.bounds;
        const auto v = visuals(arrow.id);
        Gdiplus::SolidBrush fill(blend(Gdiplus::Color(170, 13, 14, 20), Gdiplus::Color(235, 47, 40, 80), v.hover));
        g.FillEllipse(&fill, b);
        Gdiplus::Pen outline(blend(Theme::GlassOutline, Theme::AccentOutline, v.hover), 1);
        g.DrawEllipse(&outline, b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1);
        Icons::draw(g, arrow.icon, {b.X + 14, b.Y + 14, 24, 24}, blend(Theme::TextPrimary, Theme::AccentLight, v.hover), 2.2F);
        if (v.focused) drawFocusRing(g, b, b.Width / 2, Theme::AccentLight);
    }
}

std::size_t SkinsView::lastFirstRow(std::size_t cards)
{
    const auto rows = (cards + Layout::SkinColumns - 1) / Layout::SkinColumns;
    return rows > static_cast<std::size_t>(Layout::SkinRows) ? rows - Layout::SkinRows : 0;
}

namespace
{
    // A grey figure for the default skin (Mojang's own skins are not ours to ship).
    void silhouette(Gdiplus::Graphics& g, const Gdiplus::RectF& bounds, const Gdiplus::Color& color)
    {
        const float cell = std::min(bounds.Width / 16, bounds.Height / 32);
        const float x = bounds.X + (bounds.Width - 16 * cell) / 2, y = bounds.Y + (bounds.Height - 32 * cell) / 2;
        Gdiplus::SolidBrush brush(color);
        const float parts[][4] = {{4, 0, 8, 8}, {4, 8, 8, 12}, {0, 8, 4, 12}, {12, 8, 4, 12}, {4, 20, 4, 12}, {8, 20, 4, 12}};
        for (const auto& part : parts) g.FillRectangle(&brush, x + part[0] * cell, y + part[1] * cell, part[2] * cell, part[3] * cell);
    }
}

void SkinsView::draw(Gdiplus::Graphics& g, const SkinsModel& m, const ControlVisuals& visuals) const
{
    using namespace LauncherConstants;
    std::wstring subtitle = !m.message.empty() ? m.message :
        m.signedIn ? L"A skin de " + m.account + L" aparece para todo mundo no servidor." :
        L"Entre na sua Conta Eternal para trocar de skin.";
    header(g, L"Skins", subtitle, m.messageError);
    Buttons::draw(g, Layout::SkinsFolder, L"Abrir pasta", visuals(SkinFolderButtonId), Buttons::Style::Ghost, Icons::Icon::Folder);
    Buttons::draw(g, Layout::SkinsAdd, L"Adicionar skin", visuals(SkinAddButtonId), Buttons::Style::Primary, Icons::Icon::Plus);

    // Stage: the selected skin, front and back, on a lit plate.
    auto stage = Layout::SkinsStage;
    stage.Y += rise(m.entrance, 0);
    card(g, stage);
    const Gdiplus::RectF plate{stage.X + 16, stage.Y + 16, stage.Width - 32, Layout::SkinsPlateHeight};
    {
        const auto saved = g.Save();
        auto clip = createRoundedPath(plate, 16);
        g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
        Gdiplus::LinearGradientBrush sky(Gdiplus::PointF(0, plate.Y), Gdiplus::PointF(0, plate.GetBottom()),
            Gdiplus::Color(255, 34, 29, 60), Gdiplus::Color(255, 16, 16, 26));
        g.FillRectangle(&sky, plate);
        Gdiplus::GraphicsPath spot;
        spot.AddEllipse(plate.X + plate.Width / 2 - 200, plate.Y + 40, 400.0F, 320.0F);
        Gdiplus::PathGradientBrush light(&spot);
        light.SetCenterColor(Gdiplus::Color(60, 165, 130, 255));
        Gdiplus::Color edge(0, 165, 130, 255);
        int count = 1;
        light.SetSurroundColors(&edge, &count);
        g.FillPath(&light, &spot);
        // A floor shadow under each figure.
        for (const auto* figure : {&Layout::SkinsFront, &Layout::SkinsBack}) {
            Gdiplus::SolidBrush shadow(Gdiplus::Color(90, 0, 0, 0));
            g.FillEllipse(&shadow, figure->X + 20, figure->GetBottom() + stage.Y - Layout::SkinsStage.Y - 6, figure->Width - 40, 14.0F);
        }
        g.Restore(saved);
    }
    const SkinCard* selected = m.selected >= 0 && static_cast<std::size_t>(m.selected) < m.cards.size() ? &m.cards[static_cast<std::size_t>(m.selected)] : nullptr;
    const float dy = stage.Y - Layout::SkinsStage.Y;
    const Gdiplus::RectF front{Layout::SkinsFront.X, Layout::SkinsFront.Y + dy, Layout::SkinsFront.Width, Layout::SkinsFront.Height};
    const Gdiplus::RectF backView{Layout::SkinsBack.X, Layout::SkinsBack.Y + dy, Layout::SkinsBack.Width, Layout::SkinsBack.Height};
    if (selected && selected->texture) {
        SkinRenderer::drawBody(g, *selected->texture, front, m.slimChoice, false);
        SkinRenderer::drawBody(g, *selected->texture, backView, m.slimChoice, true);
    } else {
        silhouette(g, front, Gdiplus::Color(255, 58, 56, 82));
        silhouette(g, backView, Gdiplus::Color(255, 48, 46, 70));
        if (m.currentLoading && selected && selected->current)
            drawText(g, L"Carregando...", {plate.X, plate.GetBottom() - 40, plate.Width, 20}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextMuted,
                Gdiplus::StringAlignmentCenter);
    }
    drawText(g, L"FRENTE", {front.X, plate.Y + 12, front.Width, 16}, 10.5F, Gdiplus::FontStyleBold, Theme::TextMuted, Gdiplus::StringAlignmentCenter);
    drawText(g, L"COSTAS", {backView.X, plate.Y + 12, backView.Width, 16}, 10.5F, Gdiplus::FontStyleBold, Theme::TextMuted, Gdiplus::StringAlignmentCenter);
    if (m.selectedRemovable) {
        const Gdiplus::Color removeHover(255, 52, 30, 40);
        Buttons::drawIcon(g, {Layout::SkinsRemove.X, Layout::SkinsRemove.Y + dy, Layout::SkinsRemove.Width, Layout::SkinsRemove.Height},
            Icons::Icon::Trash, visuals(SkinRemoveButtonId), false, 17, &removeHover);
    }

    // Name and model of the selected skin.
    const float infoY = plate.GetBottom() + 12;
    drawDisplayText(g, selected ? selected->name : std::wstring(L"Nenhuma skin"), {stage.X + 24, infoY, stage.Width - 48, 28}, 18, Theme::TextPrimary,
        Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
    drawText(g, L"Braços", {stage.X + 24, infoY + 30, 200, 18}, 12, Gdiplus::FontStyleBold, Theme::TextSecondary);
    Buttons::draw(g, {Layout::SkinsClassic.X, Layout::SkinsClassic.Y + dy, Layout::SkinsClassic.Width, Layout::SkinsClassic.Height},
        L"Clássico (4 px)", visuals(SkinClassicButtonId), Buttons::Style::Chip, std::nullopt, !m.slimChoice, 12.5F);
    Buttons::draw(g, {Layout::SkinsSlim.X, Layout::SkinsSlim.Y + dy, Layout::SkinsSlim.Width, Layout::SkinsSlim.Height},
        L"Fino (3 px)", visuals(SkinSlimButtonId), Buttons::Style::Chip, std::nullopt, m.slimChoice, 12.5F);
    const Gdiplus::RectF use{Layout::SkinsUse.X, Layout::SkinsUse.Y + dy, Layout::SkinsUse.Width, Layout::SkinsUse.Height};
    const Gdiplus::RectF reset{Layout::SkinsReset.X, Layout::SkinsReset.Y + dy, Layout::SkinsReset.Width, Layout::SkinsReset.Height};
    if (!m.signedIn) {
        Buttons::draw(g, use, L"Entrar na Conta Eternal", visuals(SkinSignInButtonId), Buttons::Style::Primary, Icons::Icon::User, false, 14);
    } else {
        auto useVisual = visuals(SkinUseButtonId);
        const bool inUse = selected && selected->current && !m.busy;
        if (m.busy || !selected || !selected->texture) useVisual.enabled = false;
        Buttons::draw(g, use, m.busy ? L"Enviando..." : inUse ? L"Atualizar modelo dos braços" : L"Usar esta skin", useVisual,
            Buttons::Style::Primary, m.busy ? std::nullopt : std::optional<Icons::Icon>(Icons::Icon::Check), false, 14);
        auto resetVisual = visuals(SkinResetButtonId);
        if (m.busy) resetVisual.enabled = false;
        Buttons::draw(g, reset, L"Voltar para a skin padrão", resetVisual, Buttons::Style::Ghost, Icons::Icon::Refresh, false, 13);
    }

    // Library.
    auto library = Layout::SkinsLibrary;
    library.Y += rise(m.entrance, 1);
    card(g, library);
    drawText(g, L"Sua biblioteca", {library.X + 24, library.Y + 22, 300, 24}, 16, Gdiplus::FontStyleBold, Theme::TextPrimary);
    drawText(g, L"Skins que você adicionou. Escolha uma e use no servidor.", {library.X + 24, library.Y + 48, 560, 20}, 12.5F,
        Gdiplus::FontStyleRegular, Theme::TextMuted);
    const std::size_t first = m.firstRow * Layout::SkinColumns;
    for (std::size_t slot = 0; slot < Visible && first + slot < m.cards.size(); ++slot) {
        const auto index = first + slot;
        const auto& item = m.cards[index];
        auto b = Layout::skinCard(static_cast<int>(slot));
        b.Y += library.Y - Layout::SkinsLibrary.Y;
        const auto v = visuals(SkinCardFirstId + static_cast<int>(slot));
        const bool chosen = static_cast<int>(index) == m.selected;
        b.Y -= std::round(3 * v.hover);
        fillRoundedRectangle(g, b, 14, chosen ? Theme::AccentTint : blend(Theme::SurfaceSunken, Theme::SurfaceElevated, v.hover));
        const Gdiplus::RectF figure{b.X + (b.Width - 56) / 2, b.Y + 12, 56, 112};
        if (item.texture) SkinRenderer::drawBody(g, *item.texture, figure, item.slim, false);
        else silhouette(g, figure, Gdiplus::Color(255, 58, 56, 82));
        if (item.current) {
            // The skin in use says so where its name would be.
            const float width = measureText(g, L"Em uso", 12.5F, Gdiplus::FontStyleBold) + 22;
            const float x = b.X + (b.Width - width) / 2;
            Icons::draw(g, Icons::Icon::Check, {x, b.Y + 133, 13, 13}, Theme::AccentLight, 2);
            drawTextLine(g, L"Em uso", {x + 19, b.Y + 128, width, 22}, 12.5F, Gdiplus::FontStyleBold, Theme::AccentLight);
        } else {
            drawTextLine(g, item.name, {b.X + 10, b.Y + 128, b.Width - 20, 22}, 12.5F, Gdiplus::FontStyleBold,
                chosen ? Theme::TextPrimary : Theme::TextSecondary, Gdiplus::StringAlignmentCenter);
        }
        drawRoundedRectangle(g, {b.X + .5F, b.Y + .5F, b.Width - 1, b.Height - 1}, 14,
            chosen ? Theme::Accent : blend(Theme::OutlineSubtle, Theme::AccentOutline, v.hover), chosen ? 1.6F : 1.0F);
        if (v.focused) drawFocusRing(g, b, 14, Theme::AccentLight);
    }
    if (m.cards.size() <= 1) {
        const float y = Layout::skinCard(4).Y + library.Y - Layout::SkinsLibrary.Y;
        drawText(g, L"Use Adicionar skin para trazer um PNG de 64 × 64.\nSites como NameMC e The Skindex têm milhares de skins.",
            {library.X + 40, y + 40, library.Width - 80, 44}, 13, Gdiplus::FontStyleRegular, Theme::TextSecondary, Gdiplus::StringAlignmentCenter);
    }
    const auto rows = (m.cards.size() + Layout::SkinColumns - 1) / Layout::SkinColumns;
    if (rows > static_cast<std::size_t>(Layout::SkinRows)) {
        const Gdiplus::RectF list{library.X, Layout::skinCard(0).Y, library.Width - 16, 3 * 172 - 12};
        const float content = static_cast<float>(rows) * 172 - 12;
        scrollbar(g, list, content, static_cast<float>(m.firstRow) * (content - list.Height) / std::max(1.0F, static_cast<float>(lastFirstRow(m.cards.size()))));
    }
}

void HelpView::draw(Gdiplus::Graphics& g, const HelpModel& m, const ControlVisuals& visuals) const
{
    using namespace LauncherConstants;
    header(g, L"Ajuda", L"Resolva problemas comuns e encontre os arquivos do jogo.");
    struct Card { Icons::Icon icon; const wchar_t* title; const wchar_t* text; };
    const Card cards[] = {
        {Icons::Icon::Wrench, L"O jogo não abre ou falta algum arquivo?",
         L"Reparar confere Java, Minecraft, Forge e mods e baixa de novo só o que estiver faltando ou corrompido. Seus mundos e mods ficam."},
        {Icons::Icon::File, L"Logs",
         L"Registros da última instalação, do instalador do Forge e da sessão de jogo. Úteis quando algo dá errado."},
        {Icons::Icon::Bug, L"Pedir ajuda",
         L"Copie as informações do launcher (versão, Java, memória e o fim do log) e cole no grupo ou num relato no GitHub."},
        {Icons::Icon::Folder, L"Seus dados",
         L"Preferências, nickname, tempo de jogo e a instância do Minecraft ficam na pasta de dados do Eternal."}};
    for (int i = 0; i < 4; ++i) {
        auto b = Layout::helpCard(i);
        b.Y += rise(m.entrance, i);
        card(g, b);
        fillRoundedRectangle(g, {b.X + 24, b.Y + 24, 36, 36}, 10, Theme::AccentTint);
        Icons::draw(g, cards[i].icon, {b.X + 33, b.Y + 33, 18, 18}, Theme::AccentLight, 1.8F);
        drawText(g, cards[i].title, {b.X + 74, b.Y + 24, b.Width - 98, 36}, 16, Gdiplus::FontStyleBold, Theme::TextPrimary,
            Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);
        drawText(g, cards[i].text, {b.X + 24, b.Y + 76, b.Width - 48, 64}, 13, Gdiplus::FontStyleRegular, Theme::TextSecondary);
    }
    auto repair = visuals(HelpRepairButtonId);
    repair.enabled = repair.enabled && m.repairAvailable;
    const auto button = [&](int which, int index) { auto b = Layout::helpButton(which, index); b.Y += rise(m.entrance, which); return b; };
    Buttons::draw(g, button(0, 0), L"Reparar instalação", repair, Buttons::Style::Primary, Icons::Icon::Wrench);
    const struct { int id; const wchar_t* label; } logs[] = {
        {HelpInstallLogButtonId, L"Instalação"}, {HelpForgeLogButtonId, L"Forge"}, {HelpGameLogButtonId, L"Jogo"}, {HelpLogsFolderButtonId, L"Pasta"}};
    for (int i = 0; i < 4; ++i)
        Buttons::draw(g, button(1, i), logs[i].label, visuals(logs[i].id), Buttons::Style::Ghost, i == 3 ? Icons::Icon::Folder : Icons::Icon::File);
    Buttons::draw(g, button(2, 0), L"Copiar informações", visuals(HelpCopySupportButtonId), Buttons::Style::Secondary, Icons::Icon::Copy);
    Buttons::draw(g, button(2, 1), L"Relatar no GitHub", visuals(HelpReportIssueButtonId), Buttons::Style::Ghost, Icons::Icon::External);
    Buttons::draw(g, button(3, 0), L"Abrir pasta de dados", visuals(HelpDataFolderButtonId), Buttons::Style::Ghost, Icons::Icon::Folder);
    if (!m.message.empty()) {
        if (!m.messageError) Icons::draw(g, Icons::Icon::Check, {Layout::HelpMessage.X, Layout::HelpMessage.Y + 11, 16, 16}, Theme::Success, 1.8F);
        drawText(g, m.message, {Layout::HelpMessage.X + (m.messageError ? 0.0F : 26.0F), Layout::HelpMessage.Y, Layout::HelpMessage.Width, 38},
            13.5F, Gdiplus::FontStyleRegular, m.messageError ? Theme::Danger : Theme::TextPrimary, Gdiplus::StringAlignmentNear,
            Gdiplus::StringAlignmentCenter);
    }
}
