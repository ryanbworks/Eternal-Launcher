#pragma once
#include "components/Buttons.h"
#include "types/InstallationProgress.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <array>

class InstallationPanelComponent {
public:
    void draw(Gdiplus::Graphics& graphics, const InstallationTrackingState& state, float phase, const ControlVisuals& visuals,
              float fill = -1, std::array<float, 6> completion = {}) const;
    // 0..100 across the six stages, including the active transfer's share.
    static float overallPercent(const InstallationTrackingState& state, float fill = -1);
};
