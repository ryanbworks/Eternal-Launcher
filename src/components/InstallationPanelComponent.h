#pragma once
#include "types/InstallationProgress.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

class InstallationPanelComponent {
public:
    void draw(Gdiplus::Graphics& graphics,const InstallationTrackingState& state,float phase,bool cancelFocus,bool logFocus,
        float cancelHover=0,float logHover=0,bool cancelPressed=false,bool logPressed=false,float fill=-1,
        std::array<float,6> completion = {}) const;
    static Gdiplus::RectF cancelBounds() {return {572,636,184,44};}
    static Gdiplus::RectF logBounds() {return {60,636,132,44};}
};
