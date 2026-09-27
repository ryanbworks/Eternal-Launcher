#include "mocks/MockLauncherData.h"

LauncherViewState MockLauncherData::createInitialState()
{
    LauncherViewState state;
    state.operation = LauncherOperation::ReadyToPlay;
    state.headline = L"Pronto para jogar";
    state.operationMessage = L"Sua instalação está preparada para entrar no servidor.";

    // MOCK: estes estados serão substituídos pelas verificações reais da instalação,
    // dos mods e do servidor HTTP de arquivos.
    state.statusCards = {
        {L"INSTALAÇÃO", L"Pronta", L"Minecraft 1.20.1", StatusTone::Success},
        {L"MODS", L"Atualizados", L"37 arquivos verificados", StatusTone::Success},
        {L"SERVIDOR HTTP", L"Disponível", L"Resposta em 28 ms", StatusTone::Success},
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
