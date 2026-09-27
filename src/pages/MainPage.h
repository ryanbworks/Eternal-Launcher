#pragma once

#include "components/HeaderComponent.h"
#include "components/HeroComponent.h"
#include "components/NewsPanelComponent.h"
#include "components/PlayPanelComponent.h"
#include "components/PlayerProfileComponent.h"
#include "components/ProgressComponent.h"
#include "components/StatusPanelComponent.h"
#include "types/LauncherState.h"

#include <windows.h>

class MainPage
{
public:
    explicit MainPage(HINSTANCE instance);
    ~MainPage();

    bool create(int showCommand);

private:
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void paint();
    void enableDarkTitleBar() const;
    void createNicknameInput();
    void layoutNicknameInput() const;
    void handlePrimaryAction();
    void advanceMockProgress();
    void showSettingsPlaceholder() const;

    HINSTANCE instance_;
    HWND window_ = nullptr;
    HWND nicknameInput_ = nullptr;
    HFONT inputFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH inputBrush_ = nullptr;
    LauncherViewState viewState_;
    int mockProgressStep_ = 0;

    HeaderComponent header_;
    HeroComponent hero_;
    StatusPanelComponent statusPanel_;
    NewsPanelComponent newsPanel_;
    PlayerProfileComponent playerProfile_;
    PlayPanelComponent playPanel_;
    ProgressComponent progress_;
};
