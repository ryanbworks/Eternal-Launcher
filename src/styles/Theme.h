#pragma once

#include <objidl.h>
#include <gdiplus.h>

namespace Theme
{
    inline const wchar_t* FontFamily = L"Segoe UI";

    inline const Gdiplus::Color Background{255, 13, 14, 20};
    inline const Gdiplus::Color Surface{255, 22, 23, 33};
    inline const Gdiplus::Color SurfaceElevated{255, 30, 31, 44};
    inline const Gdiplus::Color SurfaceHover{255, 42, 40, 59};
    inline const Gdiplus::Color Border{255, 52, 52, 70};
    inline const Gdiplus::Color Accent{255, 165, 130, 255};
    inline const Gdiplus::Color AccentLight{255, 201, 182, 255};
    inline const Gdiplus::Color AccentInk{255, 26, 16, 49};
    inline const Gdiplus::Color Success{255, 55, 214, 144};
    inline const Gdiplus::Color Warning{255, 251, 191, 36};
    inline const Gdiplus::Color Error{255, 248, 113, 113};
    inline const Gdiplus::Color Information{255, 96, 165, 250};
    inline const Gdiplus::Color TextPrimary{255, 245, 247, 250};
    inline const Gdiplus::Color TextSecondary{255, 178, 179, 195};
    inline const Gdiplus::Color TextMuted{255, 150, 151, 172};
    inline constexpr float PanelRadius = 14.0F;
    inline constexpr float ControlRadius = 10.0F;
}
