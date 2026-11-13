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
    inline const Rect NewsHeading{100, 556, 400, 26};
    inline const Rect NewsAll{764.0F * 2 / 3 + 14, 554, 100, 30};
    inline Rect newsCard(int index) { return {100 + static_cast<float>(index) * (764.0F / 3 + 14), 590, 764.0F / 3, 186}; }
    // The leaderboard takes the slot after the two news cards.
    inline Rect rankingCard() { return newsCard(2); }
    inline const Rect RankingHeading{rankingCard().X, 556, rankingCard().Width, 26};
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
    inline const Rect InstallSummary{940, 406, 292, 108};
    inline const Rect VersionRow{944, 409, 284, 32};
    inline const Rect StatusLine{940, 532, 292, 24};
    inline const Rect StatusMessage{940, 560, 292, 46};
    inline const Rect UpdateLink{940, 606, 180, 24};
    inline const Rect Play{940, 636, 226, 58};
    inline const Rect PlayMenu{1172, 636, 60, 58};
    inline Rect quickAction(int index) { return {940 + static_cast<float>(index) * 100, 708, 92, 44}; }
    // "Something new" spotlight over the top of the landscape. It stays until read or dismissed.
    inline const Rect Spotlight{120, 86, 440, 148};
    inline const Rect SpotlightRead{140, 186, 132, 34};
    inline const Rect SpotlightDismiss{280, 186, 96, 34};
    // A play-time level reached: a card centered on the landscape.
    inline const Rect Celebration{266, 88, 460, 316};
    inline const Rect CelebrationButton{416, 344, 160, 42};
    // Every play-time level, opened from the Tempo jogado stat.
    inline const Rect Levels{124, 88, 744, 316};
    inline const Rect LevelsClose{808, 104, 44, 40};
    // Conta Eternal sign-in, centered on the landscape.
    inline const Rect Login{266, 84, 460, 324};
    inline const Rect LoginClose{670, 100, 40, 40};
    // Modes: 0 sign in, 1 first access (choose a password), 2 create an account.
    // Fields: 0 user, 1 password, 2 confirmation. Returns the field's row, or -1 if hidden.
    inline int loginRow(int mode, int field)
    {
        // Signing in leaves row 0 to the first-access hint.
        static constexpr int Rows[3][3] = {{1, 2, -1}, {-1, 0, 1}, {0, 1, 2}};
        return mode >= 0 && mode < 3 && field >= 0 && field < 3 ? Rows[mode][field] : -1;
    }
    inline Rect loginField(int row) { return {294, 196 + static_cast<float>(row) * 50, 404, 40}; }
    inline const Rect LoginSubmit{294, 362, 196, 40};
    inline const Rect LoginRegister{502, 362, 196, 40};
    inline Rect levelColumn(int index) { return {152 + static_cast<float>(index) * 140, 184, 128, 200}; }
    // Modpack changelog: takes the landscape's slot until it is acknowledged.
    inline const Rect ModpackChangesMods{636, 96, 112, 38};
    inline const Rect ModpackChangesOk{756, 96, 112, 38};
    inline Rect modpackChangesColumn(int index) { return {124 + static_cast<float>(index) * 252, 176, 240, 228}; }

    // Everything left of the player card: hero, tiles, news and ranking.
    inline const Rect LeftColumn{100, 68, 792, 708};

    // Installation panel. It takes the hero's slot while preparing, so the status
    // tiles, news and ranking below stay readable and clickable.
    inline const Rect Installation = Hero;
    inline const Rect InstallationStepper{130, 188, 732, 58};
    inline const Rect InstallationDetail{130, 256, 732, 92};
    inline const Rect InstallationLog{130, 362, 140, 42};
    inline const Rect InstallationCancel{662, 362, 200, 42};

    // Shared page header.
    inline const Rect PageTitle{100, 84, 760, 40};
    inline const Rect PageSubtitle{100, 124, 760, 22};

    // News.
    inline Rect newsEntry(int index) { return {100, 168 + static_cast<float>(index) * 152, 1156, 140}; }
    inline Rect newsLink(int index) { const auto e = newsEntry(index); return {e.GetRight() - 24 - 132, e.GetBottom() - 20 - 36, 132, 36}; }

    // Versions.
    inline Rect versionCard(int index) { return {100 + static_cast<float>(index) * 588, 168, 568, 608}; }
    inline Rect versionAction(int index) { const auto c = versionCard(index); return {c.X + 28, c.GetBottom() - 76, c.Width - 56, 52}; }

    // Mods.
    inline const Rect ModsRefresh{880, 84, 48, 44};
    inline const Rect ModsFolder{940, 84, 160, 44};
    inline const Rect ModsAdd{1112, 84, 144, 44};
    inline const Rect ModsServer{100, 168, 568, 608};
    inline const Rect ModsUser{688, 168, 568, 608};

    // Screenshots: a 4 x 3 grid of cards and a full-size viewer.
    inline const Rect ScreenshotsRefresh{1036, 84, 48, 44};
    inline const Rect ScreenshotsFolder{1096, 84, 160, 44};
    inline constexpr int ScreenshotColumns = 4;
    inline constexpr int ScreenshotRows = 3;
    inline constexpr float ScreenshotImageHeight = 156;
    inline Rect screenshotCard(int index)
    {
        return {100 + static_cast<float>(index % ScreenshotColumns) * 293, 168 + static_cast<float>(index / ScreenshotColumns) * 206, 277, 194};
    }
    // "Minhas" / "Do grupo" segmented control.
    inline const Rect ScreenshotsTabs{640, 86, 380, 40};
    inline Rect screenshotsTab(int index) { return {644 + static_cast<float>(index) * 186, 90, 186, 32}; }
    inline const Rect ViewerBack{100, 84, 116, 44};
    inline const Rect ViewerTitle{236, 80, 440, 52};
    inline const Rect ViewerShare{686, 84, 116, 44};
    inline const Rect ViewerCopy{810, 84, 116, 44};
    // A group picture: copy, then remove (own) or report (others).
    inline const Rect ViewerGroupCopy{1018, 84, 116, 44};
    inline const Rect ViewerGroupAction{1142, 84, 114, 44};
    inline const Rect ViewerFolder{934, 84, 150, 44};
    inline const Rect ViewerOpen{1092, 84, 108, 44};
    inline const Rect ViewerDelete{1208, 84, 48, 44};
    inline const Rect ViewerImage{100, 148, 1156, 628};
    inline const Rect ViewerPrevious{120, 434, 52, 52};
    inline const Rect ViewerNext{1184, 434, 52, 52};

    // Skins: the selected skin on a stage at the left, the library at the right.
    inline const Rect SkinsFolder{940, 84, 160, 44};
    inline const Rect SkinsAdd{1112, 84, 144, 44};
    inline const Rect SkinsStage{100, 168, 440, 608};
    inline constexpr float SkinsPlateHeight = 344;
    inline const Rect SkinsFront{154, 212, 144, 288};
    inline const Rect SkinsBack{342, 212, 144, 288};
    inline const Rect SkinsRemove{476, 192, 40, 40};
    // Share (Minhas) in the opposite corner; report or remove (Explorar) in the remove button's place.
    inline const Rect SkinsShare{124, 192, 40, 40};
    // "Minhas skins" / "Explorar" segmented control, and the Explorar refresh button.
    inline const Rect SkinsTabs{548, 86, 380, 40};
    inline Rect skinsTab(int index) { return {552 + static_cast<float>(index) * 186, 90, 186, 32}; }
    inline const Rect SkinsRefresh{1208, 84, 48, 44};
    inline const Rect SkinsClassic{124, 594, 192, 38};
    inline const Rect SkinsSlim{324, 594, 192, 38};
    inline const Rect SkinsUse{124, 650, 392, 48};
    inline const Rect SkinsReset{124, 708, 392, 40};
    inline const Rect SkinsLibrary{560, 168, 696, 608};
    inline constexpr int SkinColumns = 4;
    inline constexpr int SkinRows = 3;
    inline Rect skinCard(int index)
    {
        return {584 + static_cast<float>(index % SkinColumns) * 165, 252 + static_cast<float>(index / SkinColumns) * 172, 153, 160};
    }

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
