#include "app/LauncherApplication.h"

#include <windows.h>

// Windows GUI entry point. Application startup and lifecycle are
// encapsulated elsewhere to keep this file intentionally simple.
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    LauncherApplication application(instance);
    return application.run(showCommand);
}
