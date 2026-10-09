#include "app/LauncherApplication.h"

#include <windows.h>

// Ponto de entrada gráfico do Windows. A inicialização e o ciclo de vida da
// aplicação ficam encapsulados para manter este arquivo propositalmente simples.
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    LauncherApplication application(instance);
    return application.run(showCommand);
}
