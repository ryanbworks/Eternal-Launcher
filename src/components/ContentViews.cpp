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
        drawText(g, item.date, {pill.GetRight() + 12, pill.Y, 260, 22}, 12, Gdiplus::FontStyleRegular, Theme::TextMuted,
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
