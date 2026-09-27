#pragma once

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

    HINSTANCE instance_;
    HWND window_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    LauncherViewState viewState_;
};
