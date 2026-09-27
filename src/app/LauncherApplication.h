#pragma once

#include <windows.h>

class LauncherApplication
{
public:
    explicit LauncherApplication(HINSTANCE instance);
    int run(int showCommand);

private:
    HINSTANCE instance_;
};
