#pragma once

#include "types/LauncherState.h"

#include <objidl.h>
#include <gdiplus.h>

class NewsPanelComponent
{
public:
    void draw(Gdiplus::Graphics& graphics, const Gdiplus::RectF& bounds,
              const NewsItem& news) const;
};
