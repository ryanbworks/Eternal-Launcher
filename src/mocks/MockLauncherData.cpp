#include "mocks/MockLauncherData.h"

LauncherViewState MockLauncherData::createInitialState()
{
    LauncherViewState state;
    state.operation = LauncherOperation::CheckingUpdates;
    state.headline = L"Conectando ao servidor";
    state.operationMessage = L"Buscando o manifesto no servidor HTTP de arquivos...";
    state.primaryActionLabel = L"AGUARDE...";
    state.playEnabled = false;

    // These values are placeholders only until the asynchronous checks
    // replace the three cards with real results.
    state.statusCards = {
        {L"INSTALAÇÃO", L"Verificando...", L"Java, Minecraft e Forge", StatusTone::Information},
        {L"ARQUIVOS", L"Aguardando", L"Manifesto privado ainda não lido", StatusTone::Warning},
        {L"SERVIDOR HTTP", L"Conectando...", L"Buscando manifesto real", StatusTone::Information},
    };

    // MOCK: temporary editorial content to validate the news card.
    state.featuredNews = {
        L"AVISO DO SERVIDOR",
        L"O mundo está esperando por você",
        L"Fresquin",
        L"Hoje"
    };

    return state;
}
