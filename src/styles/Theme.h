#pragma once

#include <objidl.h>
#include <gdiplus.h>

namespace Theme
{
    inline const wchar_t* FontFamily = L"Segoe UI";

    inline const Gdiplus::Color Background{255, 11, 13, 18};
    inline const Gdiplus::Color Surface{255, 19, 22, 30};
    inline const Gdiplus::Color SurfaceElevated{255, 25, 29, 39};
    inline const Gdiplus::Color SurfaceHover{255, 32, 37, 49};
    inline const Gdiplus::Color Border{255, 43, 49, 64};
    inline const Gdiplus::Color Accent{255, 139, 92, 246};
    inline const Gdiplus::Color AccentLight{255, 167, 139, 250};
    inline const Gdiplus::Color Success{255, 55, 214, 144};
    inline const Gdiplus::Color Warning{255, 251, 191, 36};
    inline const Gdiplus::Color Error{255, 248, 113, 113};
    inline const Gdiplus::Color Information{255, 96, 165, 250};
    inline const Gdiplus::Color TextPrimary{255, 245, 247, 250};
    inline const Gdiplus::Color TextSecondary{255, 156, 163, 175};
    inline const Gdiplus::Color TextMuted{255, 101, 109, 126};
}
