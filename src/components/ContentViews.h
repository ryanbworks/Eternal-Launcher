#pragma once

#include "components/Buttons.h"
#include "services/ModInventory.h"
#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

#include <string>
#include <vector>

class HeroComponent;

struct NewsModel
{
    const std::vector<NewsItem>* items = nullptr;
    std::size_t first = 0;
    bool loading = false;
    std::wstring error;
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

struct HelpModel
{
    std::wstring message;
    bool messageError = false;
    bool repairAvailable = true;
};

class HelpView
{
public:
    void draw(Gdiplus::Graphics& graphics, const HelpModel& model, const ControlVisuals& visuals) const;
};
