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
        Back=1200, MinRam, MaxRam, Width, Height, Fullscreen, Minimize,
        JavaAuto, JavaCustom, JavaPath, JavaBrowse, GamePath, GameBrowse,
        OpenGame, OpenMods, OpenLogs, Restore, Cancel, Save,
        RamSlider, Ram4, Ram6, Ram8, ControlEnd
    };
    ~SettingsPage();
    bool create(HWND parent,HINSTANCE instance);
    void open(const LauncherSettings& settings,std::wstring_view warning);
    void show(bool visible);
    void layout(float scale,float offsetX,float offsetY);
    void setTransitionOffset(int pixels);
    void setLocked(bool locked);
    void invalidateButtons() const;
    void draw(Gdiplus::Graphics& graphics,bool preview=false) const;
    void handle(int id,int notification);
    void handleMemoryScroll(HWND slider);
    bool collect(LauncherSettings& settings);
    void setMessage(std::wstring message,bool error=false);
    int focusedControlId() const;
    static bool containsId(int id) { return id>=Back && id<ControlEnd; }
private:
    static Gdiplus::RectF bounds(int id);
    static bool isEdit(int id);
    static LRESULT CALLBACK controlProcedure(HWND window,UINT message,WPARAM wParam,
        LPARAM lParam,UINT_PTR id,DWORD_PTR data);
    HWND control(int id) const { return controls_[static_cast<std::size_t>(id-Back)]; }
    void populate(const LauncherSettings& settings);
    void syncMemoryControls(int megabytes);
    void selectMemory(int megabytes);
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
    std::array<HWND,ControlEnd-Back> controls_{};
    std::array<bool,ControlEnd-Back> hover_{};
    std::array<RECT,ControlEnd-Back> layoutBounds_{};
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
};
