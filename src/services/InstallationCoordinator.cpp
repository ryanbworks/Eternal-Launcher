#include "services/InstallationCoordinator.h"
#include <windows.h>
#include <fstream>
#include <iomanip>

namespace {
    std::string utf8(std::wstring_view value) {
        const int size=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
        std::string result(size,'\0');
        if(size) WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),size,nullptr,nullptr);
        return result;
    }
    // Do not persist URLs, tokens or authentication payloads supplied by a remote error.
    std::wstring safeDetail(std::wstring text) {
        for(const auto marker:{L"http://",L"https://",L"access_token",L"refresh_token",L"Authorization",L"Bearer "}) {
            auto at=text.find(marker);
            if(at!=std::wstring::npos) text.erase(at);
        }
        return text;
    }
    const wchar_t* recovery(InstallationFailure category) {
        switch(category) {
        case InstallationFailure::Network:return L"A conexão foi interrompida. Confira sua internet e tente novamente.";
        case InstallationFailure::Http:return L"O serviço de arquivos não respondeu como esperado. Tente novamente mais tarde.";
        case InstallationFailure::Disk:return L"Não foi possível gravar os arquivos. Confira espaço livre e permissões da pasta.";
        case InstallationFailure::Integrity:return L"Um arquivo não passou na verificação de integridade. Tente novamente para baixá-lo outra vez.";
        case InstallationFailure::Configuration:return L"Confira as configurações do jogo. Se persistir, abra o log e contate o responsável pelo launcher.";
        default:return L"Não foi possível concluir a preparação. Tente novamente ou abra o log para consultar os detalhes.";
        }
    }
}
void InstallationCoordinator::recordLaunchFailure(const LauncherSettings& settings,std::wstring_view detail) {
    std::ofstream log(LauncherSettingsStore::effectiveGameDirectory(settings)/L"logs"/L"installation.log",std::ios::binary|std::ios::app);
    if(log) log<<"Falha ao iniciar Minecraft: "<<utf8(safeDetail(std::wstring(detail)))<<'\n';
}
InstallationResult InstallationCoordinator::run(std::stop_token stop,const InstallationProgressCallback& callback) const {
    std::wstring settingsError;
    if(!LauncherSettingsStore::validate(settings_,settingsError)) {
        if(stop.stop_requested()) return {InstallationOutcome::Cancelled,InstallationFailure::None,L"Instalação cancelada. Os arquivos completos serão reaproveitados."};
        return {InstallationOutcome::Failed,InstallationFailure::Configuration,recovery(InstallationFailure::Configuration),settingsError};
    }
    std::ofstream log;
    try {
        const auto game=LauncherSettingsStore::effectiveGameDirectory(settings_);
        std::filesystem::create_directories(game);
        if(!std::filesystem::exists(game/L"eternal-instance.json")) {
            std::ofstream marker(game/L"eternal-instance.json",std::ios::binary);
            marker<<"{\"launcher\":\"Eternal\",\"schemaVersion\":1}\n";
        }
        const auto directory=game/L"logs";
        std::filesystem::create_directories(directory);
        log.open(directory/L"installation.log",std::ios::binary|std::ios::app);
    } catch(const std::exception&) { /* Logging must not prevent recovery. */ }
    if(log) {
        SYSTEMTIME time{};GetSystemTime(&time);
        log<<"\n[Eternal] Instalação "<<time.wYear<<'-'<<std::setw(2)<<std::setfill('0')<<time.wMonth
            <<'-'<<std::setw(2)<<time.wDay<<'T'<<std::setw(2)<<time.wHour<<':'<<std::setw(2)<<time.wMinute<<':'<<std::setw(2)<<time.wSecond<<"Z\n";
    }
    std::optional<InstallationProgress> previous;
    const auto report=[&](const InstallationProgress& value) {
        if(log && (!previous || previous->stage!=value.stage || previous->activity!=value.activity || previous->state!=value.state)) {
            log<<utf8(installationStageLabel(value.stage))<<": "<<utf8(safeDetail(value.message))<<'\n';log.flush();
        }
        previous=value;
        if(callback) callback(value);
    };
    const auto finish=[&](InstallationResult result) {
        if(stop.stop_requested()) {
            result.outcome=InstallationOutcome::Cancelled;result.failure=InstallationFailure::None;
            result.message=L"Instalação cancelada. Os arquivos completos serão reaproveitados.";
        }
        if(log) {log<<"Resultado: "<<static_cast<int>(result.outcome)<<"; categoria: "<<static_cast<int>(result.failure)<<'\n'
            <<utf8(safeDetail(result.technicalDetail))<<'\n';log.flush();}
        return result;
    };
    const auto fail=[&](InstallationFailure failure,std::wstring detail) {
        if(failure==InstallationFailure::None) failure=InstallationFailure::Configuration;
        return finish({InstallationOutcome::Failed,failure,recovery(failure),safeDetail(std::move(detail)),0});
    };
    try {
        report({InstallationStage::Java,InstallationActivity::Checking,InstallationStepState::Active,L"Consultando os arquivos do grupo..."});
        if(stop.stop_requested()) return finish({});
        const auto manifest=files_.fetchManifest(stop);
        if(!manifest.success) return fail(manifest.failure,manifest.errorMessage);
        if(stop.stop_requested()) return finish({});
        const auto local=launcher_.validateInstallation();
        if(local.issue==MinecraftLaunchIssue::InvalidConfiguration) return fail(InstallationFailure::Configuration,local.message);
        if(local.issue==MinecraftLaunchIssue::JavaMissing) {
            const auto java=java_.ensureInstalled(stop,report);
            if(!java.success) return fail(java.failure,java.message);
        } else report({InstallationStage::Java,InstallationActivity::Checking,InstallationStepState::Available,L"Java 17 já disponível."});
        if(stop.stop_requested()) return finish({});
        const auto game=game_.ensureInstalled(manifest.content,stop,report);
        if(!game.success) return fail(game.failure,game.message);
        if(stop.stop_requested()) return finish({});
        report({InstallationStage::Validation,InstallationActivity::Verifying,InstallationStepState::Active,L"Validando a instalação antes de iniciar..."});
        const auto ready=launcher_.validateInstallation();
        if(!ready.success) return fail(ready.issue==MinecraftLaunchIssue::InvalidConfiguration ? InstallationFailure::Configuration : InstallationFailure::Integrity,ready.message);
        if(stop.stop_requested()) return finish({});
        report({InstallationStage::Validation,InstallationActivity::Verifying,InstallationStepState::Complete,L"Instalação pronta. Iniciando Minecraft..."});
        return finish({InstallationOutcome::Success,InstallationFailure::None,L"Instalação concluída e verificada.",{},game.synchronizedFiles});
    } catch(const std::exception&) {return fail(InstallationFailure::Disk,L"Falha ao acessar os arquivos da instalação.");}
}
