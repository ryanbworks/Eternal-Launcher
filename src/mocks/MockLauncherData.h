#pragma once

#include "types/LauncherState.h"

#include <array>

namespace MockLauncherData
{
    struct ProgressStep
    {
        LauncherOperation operation;
        int progress;
        const wchar_t* headline;
        const wchar_t* message;
    };

    // Cria os dados temporários utilizados exclusivamente na primeira versão visual.
    LauncherViewState createInitialState();

    // MOCK: descreve a sequência visual acionada pelo botão Jogar.
    std::array<ProgressStep, 5> createProgressSequence();
}
