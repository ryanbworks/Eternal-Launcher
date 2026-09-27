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
