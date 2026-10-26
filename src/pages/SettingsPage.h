#pragma once
#include "config/LauncherSettings.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <array>

class SettingsPage
{
public:
    enum Id {
        TabGeneral=1200, TabAdvanced, TabAbout,
        MinRam, MaxRam, Width, Height, Fullscreen, Minimize,
        JavaAuto, JavaCustom, JavaPath, JavaBrowse, GamePath, GameBrowse,
        OpenGame, OpenMods, OpenLogs, Restore, Cancel, Save,
        RamSlider, Ram4, Ram6, Ram8, OpenChangelog, OpenRepository, OpenDataFolder, ControlEnd
    };
    enum class Tab { General, Advanced, About };
    ~SettingsPage();
    bool create(HWND parent,HINSTANCE instance);
    void open(const LauncherSettings& settings,std::wstring_view warning);
    void show(bool visible);
    void layout(float scale,float offsetX,float offsetY);
    void setLocked(bool locked);
    void setTab(Tab tab);
    [[nodiscard]] Tab tab() const { return tab_; }
    void invalidateButtons() const;
    void draw(Gdiplus::Graphics& graphics,bool preview=false) const;
    void handle(int id,int notification);
    void handleMemoryScroll(HWND slider);
    bool collect(LauncherSettings& settings);
    void setMessage(std::wstring message,bool error=false);
    int focusedControlId() const;
    // Physical-memory based suggestion: 4, 6 or 8 GB.
    static int recommendedMemory();
    static bool containsId(int id) { return id>=TabGeneral && id<ControlEnd; }
    // Moves the switch knobs toward their values; returns true while any is still travelling.
    bool advance(float milliseconds);
    void settle();
private:
    static Gdiplus::RectF bounds(int id);
    static bool isEdit(int id);
    static Tab tabOf(int id);
    static bool isAlwaysVisible(int id);
    static LRESULT CALLBACK controlProcedure(HWND window,UINT message,WPARAM wParam,
        LPARAM lParam,UINT_PTR id,DWORD_PTR data);
    HWND control(int id) const { return controls_[static_cast<std::size_t>(id-TabGeneral)]; }
    void populate(const LauncherSettings& settings);
    void syncMemoryControls(int megabytes);
    void selectMemory(int megabytes);
    void applyVisibility();
    void drawMemorySlider(Gdiplus::Graphics& graphics,const Gdiplus::RectF& bounds,bool focused) const;
    static std::wstring memoryLabel(int megabytes);
    void browseJava();
    void browseFolder();
    void openFolder(const std::filesystem::path& path);
    std::wstring text(int id) const;
    void redraw() const;
    HWND parent_=nullptr;
    HINSTANCE instance_=nullptr;
    HFONT font_=nullptr;
    std::array<HWND,ControlEnd-TabGeneral> controls_{};
    std::array<bool,ControlEnd-TabGeneral> hover_{};
    LauncherSettings saved_;
    LauncherSettings draft_;
    std::wstring message_;
    bool error_=false;
    bool automaticJava_=true;
    bool locked_=false;
    bool visible_=false;
    bool updatingMemory_=false;
    int sliderMaximumSteps_=32;
    float controlScale_=1;
    Tab tab_=Tab::General;
    std::array<float,2> knobs_{};  // Fullscreen, tray
    float tabIndicator_=0;          // 0 General, 1 Advanced, 2 About
};
