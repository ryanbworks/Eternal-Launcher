#pragma once

#include <objidl.h>
#include <gdiplus.h>

// Logical coordinates of the 1280 x 800 canvas. The title bar and sidebar are
// anchored to the window edges; everything else lives in the content area.
namespace Layout
{
    using Rect = Gdiplus::RectF;

    inline constexpr float ContentLeft = 100;
    inline constexpr float ContentTop = 68;
    inline constexpr float ContentRight = 1256;
    inline constexpr float ContentBottom = 776;
    inline constexpr float ContentWidth = ContentRight - ContentLeft;
    inline const Rect Content{ContentLeft, ContentTop, ContentWidth, ContentBottom - ContentTop};

    // Window chrome (title bar and sidebar coordinates use the chrome transform).
    inline Rect captionButton(int index, float windowWidth) { return {windowWidth - 46.0F * static_cast<float>(3 - index), 0, 46, 44}; }
    inline Rect navButton(int index) { return {14, 60 + static_cast<float>(index) * 56, 48, 48}; }
    inline Rect sidebarHelp(float windowHeight) { return {14, windowHeight - 64, 48, 48}; }

    // Home.
    inline const Rect Hero{100, 68, 792, 356};
    inline const Rect HeroGlass{120, 252, 440, 152};
    inline Rect tile(int index) { return {100 + static_cast<float>(index) * (764.0F / 3 + 14), 444, 764.0F / 3, 92}; }
    inline const Rect NewsHeading{100, 556, 600, 26};
    inline const Rect NewsAll{792, 554, 100, 30};
    inline Rect newsCard(int index) { return {100 + static_cast<float>(index) * (764.0F / 3 + 14), 590, 764.0F / 3, 186}; }
    inline const Rect Aside{916, 68, 340, 708};
    inline const Rect Avatar{940, 92, 68, 68};
    inline const Rect NicknameKicker{1024, 94, 208, 16};
    inline const Rect NicknameField{1024, 112, 208, 40};
    inline const Rect NicknameEdit{1024, 116, 166, 32};
    inline const Rect EditNickname{1198, 114, 34, 34};
    inline const Rect NicknameStatus{1024, 158, 208, 18};
    inline const Rect ModeLabel{940, 192, 292, 18};
    inline const Rect OfflineMode{940, 216, 143, 98};
    inline const Rect OriginalMode{1089, 216, 143, 98};
    inline Rect stat(int index) { return {940 + static_cast<float>(index) * 149, 330, 143, 60}; }
    inline const Rect StatusLine{940, 532, 292, 24};
    inline const Rect StatusMessage{940, 560, 292, 46};
    inline const Rect UpdateLink{940, 606, 180, 24};
    inline const Rect Play{940, 636, 226, 58};
    inline const Rect PlayMenu{1172, 636, 60, 58};
    inline Rect quickAction(int index) { return {940 + static_cast<float>(index) * 100, 708, 92, 44}; }
    inline const Rect Toast{546, 84, 330, 40};
    inline const Rect ToastButton{758, 89, 110, 30};

    // Installation panel (replaces the left column while preparing).
    inline const Rect Installation{100, 68, 792, 708};
    inline const Rect InstallationLog{130, 700, 140, 46};
    inline const Rect InstallationCancel{662, 700, 200, 46};

    // Shared page header.
    inline const Rect PageTitle{100, 84, 760, 40};
    inline const Rect PageSubtitle{100, 124, 760, 22};

    // News.
    inline Rect newsEntry(int index) { return {100, 168 + static_cast<float>(index) * 152, 1156, 140}; }
    inline Rect newsLink(int index) { const auto e = newsEntry(index); return {e.GetRight() - 24 - 132, e.GetBottom() - 20 - 36, 132, 36}; }

    // Mods.
    inline const Rect ModsRefresh{880, 84, 48, 44};
    inline const Rect ModsFolder{940, 84, 160, 44};
    inline const Rect ModsAdd{1112, 84, 144, 44};
    inline const Rect ModsServer{100, 168, 568, 608};
    inline const Rect ModsUser{688, 168, 568, 608};

    // Help.
    inline Rect helpCard(int index) { return {100 + static_cast<float>(index % 2) * 588, 168 + static_cast<float>(index / 2) * 236, 568, 220}; }
    inline const Rect HelpMessage{100, 648, 1156, 40};
    inline Rect helpButton(int card, int index)
    {
        const auto c = helpCard(card);
        switch (card) {
        case 1: return {c.X + 24 + static_cast<float>(index) * 130, c.Y + 152, 122, 44};
        case 2: return index == 0 ? Rect{c.X + 24, c.Y + 152, 256, 44} : Rect{c.X + 292, c.Y + 152, 200, 44};
        default: return {c.X + 24, c.Y + 152, 220, 44};
        }
    }
}
