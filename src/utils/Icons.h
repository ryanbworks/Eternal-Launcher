#pragma once

#include <objidl.h>
#include <gdiplus.h>

// Stroke icons drawn on a 24-unit grid, scaled to the requested bounds.
namespace Icons
{
    enum class Icon {
        Home, News, Mods, Settings, Help, Pencil, Check, User, Key, Folder, Wrench,
        File, Copy, External, ChevronDown, ChevronRight, Play, Minimize, Maximize, Restore, Close,
        Plus, Refresh, Shield, Bug, Clock, Calendar, Server, Download, Trash, Power
    };
    void draw(Gdiplus::Graphics& graphics, Icon icon, const Gdiplus::RectF& bounds,
              const Gdiplus::Color& color, float strokeWidth = 1.8F);
}
