#include "mocks/MockLauncherData.h"

LauncherViewState MockLauncherData::createInitialState()
{
    LauncherViewState state;
    state.operation = LauncherOperation::CheckingUpdates;
    state.headline = L"Conectando ao servidor";
    state.operationMessage = L"Buscando o manifesto no servidor HTTP de arquivos...";
    state.playEnabled = false;

    // MOCK: instalação e mods continuam simulados. O terceiro cartão é substituído
    // pelo resultado real do HttpFileService assim que a tela é aberta.
    state.statusCards = {
        {L"INSTALAÇÃO", L"Pronta", L"Minecraft 1.20.1", StatusTone::Success},
        {L"MODS", L"Atualizados", L"37 arquivos verificados", StatusTone::Success},
        {L"SERVIDOR HTTP", L"Conectando...", L"Buscando manifesto real", StatusTone::Information},
    };

    // MOCK: conteúdo editorial temporário para validar o cartão de notícias.
    state.featuredNews = {
        L"AVISO DO SERVIDOR",
        L"O mundo está esperando por você",
        L"A primeira versão visual do Eternal Launcher já está pronta para testes.",
        L"Hoje"
    };

    return state;
}
