#include "mocks/MockLauncherData.h"

LauncherViewState MockLauncherData::createInitialState()
{
    LauncherViewState state;
    state.operation = LauncherOperation::CheckingUpdates;
    state.headline = L"Conectando ao servidor";
    state.operationMessage = L"Buscando o manifesto no servidor HTTP de arquivos...";
    state.primaryActionLabel = L"AGUARDE...";
    state.playEnabled = false;

    // Estes valores são placeholders somente até as verificações assíncronas
    // substituírem os três cartões por resultados reais.
    state.statusCards = {
        {L"INSTALAÇÃO", L"Verificando...", L"Java, Minecraft e Forge", StatusTone::Information},
        {L"ARQUIVOS", L"Aguardando", L"Manifesto privado ainda não lido", StatusTone::Warning},
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
