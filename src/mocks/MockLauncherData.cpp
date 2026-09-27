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

std::array<MockLauncherData::ProgressStep, 5> MockLauncherData::createProgressSequence()
{
    // MOCK: os percentuais e textos existem apenas para validar a transição dos estados.
    return {{
        {LauncherOperation::CheckingMods, 28, L"Verificando mods", L"Comparando a lista de mods simulada..."},
        {LauncherOperation::CheckingUpdates, 51, L"Consultando atualizações", L"Conectando ao servidor HTTP simulado..."},
        {LauncherOperation::DownloadingFiles, 73, L"Baixando arquivos", L"Simulando o recebimento de arquivos atualizados..."},
        {LauncherOperation::PreparingMinecraft, 91, L"Preparando Minecraft", L"Organizando os argumentos de inicialização simulados..."},
        {LauncherOperation::UpdateCompleted, 100, L"Pronto para jogar", L"Simulação concluída. Nenhum arquivo real foi alterado."}
    }};
}
