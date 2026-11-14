#pragma once

#include "components/Buttons.h"
#include "services/ModInventory.h"
#include "components/SkinRenderer.h"
#include "services/ScreenshotLibrary.h"
#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

class HeroComponent;

struct NewsModel
{
    const std::vector<NewsItem>* items = nullptr;
    std::size_t first = 0;
    bool loading = false;
    std::wstring error;
    float entrance = 1;  // 0..1 staggered rise of the entries
};

class NewsView
{
public:
    static constexpr std::size_t Visible = 4;
    void draw(Gdiplus::Graphics& graphics, const NewsModel& model, const ControlVisuals& visuals) const;
};

struct ModsModel
{
    const ModInventorySnapshot* inventory = nullptr;
    float serverScroll = 0;
    float userScroll = 0;
    int hoveredRow = -1;
    int hoveredAction = -1; // 0 = switch, 1 = remove
    std::wstring message;
    bool messageError = false;
    bool locked = false;
    // Switch knob positions by mod name (0 off .. 1 on) while they slide.
    const std::map<std::wstring, float>* knobs = nullptr;
    float entrance = 1;  // 0..1 cascade of the rows
};

class ModsView
{
public:
    enum class Action { None, Toggle, Remove };
    struct Hit { int row = -1; Action action = Action::None; };
    static constexpr float ServerRowHeight = 40;
    static constexpr float UserRowHeight = 60;
    void draw(Gdiplus::Graphics& graphics, const ModsModel& model, const ControlVisuals& visuals) const;
    static Hit hitTest(const ModsModel& model, float x, float y);
    static float maximumServerScroll(const ModsModel& model);
    static float maximumUserScroll(const ModsModel& model);
    static Gdiplus::RectF serverList();
    static Gdiplus::RectF userList();
};

struct VersionsModel
{
    bool vanillaSelected = false;
    bool vanillaInstalled = false;
    int serverMods = -1;
    float entrance = 1;
};

class VersionsView
{
public:
    void draw(Gdiplus::Graphics& graphics, const VersionsModel& model, const HeroComponent& hero, const ControlVisuals& visuals) const;
};

// One card of the Capturas grid: a local screenshot or a picture of the group gallery.
struct ShotCard
{
    std::wstring key;     // thumbnail key: the file path, or the gallery id
    std::wstring title;   // bold line under the picture
    std::wstring detail;  // muted text at the right of that line
    std::wstring badge;   // chip over the picture ("1.8.9", "Sua")
    bool shared = false;  // a local picture already in the group gallery
};

struct ScreenshotsModel
{
    bool group = false;   // the group gallery tab instead of the player's own captures
    std::vector<ShotCard> cards;
    // Decoded thumbnails by card key; a missing one draws a placeholder while it loads.
    const std::map<std::wstring, std::shared_ptr<Gdiplus::Bitmap>>* thumbnails = nullptr;
    std::size_t firstRow = 0;
    // Index of the picture open in the viewer, or -1 for the grid.
    int viewer = -1;
    const Gdiplus::Bitmap* viewerImage = nullptr;
    int viewerWidth = 0;
    int viewerHeight = 0;
    std::wstring viewerTitle;
    std::wstring viewerDetail;
    std::wstring message;
    bool messageError = false;
    // Group gallery state.
    bool loading = false;         // the list is on its way
    bool unavailable = false;     // the server did not answer
    bool sharing = false;         // an upload is running
    bool viewerShared = false;    // the open local picture is already in the gallery
    bool viewerMine = false;      // the open gallery picture was shared from here
    float entrance = 1;
    float time = 0;
};

class ScreenshotsView
{
public:
    static constexpr std::size_t Visible = 12;
    void draw(Gdiplus::Graphics& graphics, const ScreenshotsModel& model, const ControlVisuals& visuals) const;
    static std::size_t rowCount(std::size_t entries);
    static std::size_t lastFirstRow(std::size_t entries);
private:
    void drawGrid(Gdiplus::Graphics& graphics, const ScreenshotsModel& model, const ControlVisuals& visuals) const;
    void drawViewer(Gdiplus::Graphics& graphics, const ScreenshotsModel& model, const ControlVisuals& visuals) const;
};

// One skin of the Skins page: the account's current skin or one of the library.
struct SkinCard
{
    std::wstring name;
    const SkinTexture* texture = nullptr;  // null: the default skin (or still loading)
    bool slim = false;
    bool current = false;                  // the skin the account uses right now
    bool group = false;                    // Explorar: a player's skin on the server
    bool searched = false;                 // Explorar: found by nickname
    bool featured = false;                 // Explorar: from the Populares catalog
};

struct SkinsModel
{
    std::vector<SkinCard> cards;
    int selected = 0;
    std::size_t firstRow = 0;
    bool signedIn = false;
    std::wstring account;
    bool currentLoading = false;
    bool busy = false;          // an upload or reset is running
    bool slimChoice = false;    // model chosen for the selected skin
    bool selectedRemovable = false;
    std::wstring message;
    bool messageError = false;
    float entrance = 1;
    float time = 0;
    // Explorar: the group's skins and the ones the community shared.
    bool explore = false;
    bool exploreLoading = false;
    bool exploreUnavailable = false;  // the community list did not come
    std::wstring selectedDetail;      // who uses or shared the selected skin
    bool selectedShareable = false;   // Minhas: a library skin, not shared yet
    bool selectedShared = false;      // Minhas: already in Explorar
    bool selectedMine = false;        // Explorar: shared from this computer
    bool selectedCommunity = false;   // Explorar: a shared skin (reportable)
    bool sharing = false;             // a share, removal or report is running
    bool searching = false;           // a nickname search is running
    std::wstring searchPreview;       // previews only: text drawn in the search field
    int filter = 0;                   // 0 all, 1 group, 2 shared, 3 popular
    std::array<std::size_t, 4> filterCounts{};
    float cardsEntrance = 1;          // 0..1 cascade of the cards (tab, filter, list arriving)
    float turn = 0;                   // the stage figure's rotation, radians (0 faces the player)
};

class SkinsView
{
public:
    static constexpr std::size_t Visible = 12;
    void draw(Gdiplus::Graphics& graphics, const SkinsModel& model, const ControlVisuals& visuals) const;
    static std::size_t lastFirstRow(std::size_t cards);
};

struct HelpModel
{
    std::wstring message;
    bool messageError = false;
    bool repairAvailable = true;
    float entrance = 1;
};

class HelpView
{
public:
    void draw(Gdiplus::Graphics& graphics, const HelpModel& model, const ControlVisuals& visuals) const;
};
