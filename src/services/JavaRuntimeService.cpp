#include "services/JavaRuntimeService.h"

#include "config/LauncherConfig.h"
#include "services/HttpDownloadClient.h"
#include "services/JavaRuntimePackage.h"
#include "services/PortableJavaInstaller.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <exception>

namespace
{
    std::wstring readEnvironmentVariable(const wchar_t* name)
    {
        wchar_t* value = nullptr;
        std::size_t length = 0;
        if (_wdupenv_s(&value, &length, name) != 0 || value == nullptr)
        {
            return {};
        }
        std::wstring result(value);
        std::free(value);
        return result;
    }

    std::wstring utf8ToWide(std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }
        const int size = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        std::wstring converted(static_cast<std::size_t>(std::max(size, 0)), L'\0');
        if (size > 0)
        {
            MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                converted.data(), size);
        }
        return converted;
    }
}

bool JavaRuntimeService::isInstalled() const
{
    return PortableJavaInstaller::isJava17Runtime(runtimeDirectory());
}

std::filesystem::path JavaRuntimeService::runtimeDirectory() const
{
    return dataDirectory() / L"runtime";
}

JavaRuntimeResult JavaRuntimeService::ensureInstalled(
    std::stop_token stopToken,
    const JavaRuntimeProgressCallback& progress)
{
    auto result=install(stopToken, progress, false);
    result.outcome=stopToken.stop_requested() ? InstallationOutcome::Cancelled : result.success ? InstallationOutcome::Success : InstallationOutcome::Failed;
    if(result.outcome==InstallationOutcome::Cancelled) {result.success=false;result.message=L"Instalação do Java cancelada.";result.failure=InstallationFailure::None;}
    return result;
}

JavaRuntimeResult JavaRuntimeService::reinstall(
    std::stop_token stopToken,
    const JavaRuntimeProgressCallback& progress)
{
    auto result=install(stopToken, progress, true);
    result.outcome=stopToken.stop_requested() ? InstallationOutcome::Cancelled : result.success ? InstallationOutcome::Success : InstallationOutcome::Failed;
    if(result.outcome==InstallationOutcome::Cancelled) {result.success=false;result.message=L"Instalação do Java cancelada.";result.failure=InstallationFailure::None;}
    return result;
}

JavaRuntimeResult JavaRuntimeService::install(
    std::stop_token stopToken,
    const JavaRuntimeProgressCallback& progress,
    bool force)
{
    if (!force && isInstalled())
    {
        if(progress) progress({InstallationStage::Java,InstallationActivity::Checking,InstallationStepState::Available,L"Java 17 já disponível."});
        return {true, false, L"Java 17 portátil já está disponível."};
    }

    const auto dataPath = dataDirectory();
    const auto downloadPath = dataPath / L"downloads" / L"temurin-17.zip.part";
    const auto report = [&progress](InstallationActivity activity, std::wstring_view message)
    {
        if (progress)
        {
            progress({InstallationStage::Java,activity,InstallationStepState::Active,std::wstring(message)});
        }
    };
    try
    {
        report(InstallationActivity::Checking, L"Consultando a versão portátil do Java 17...");
        HttpDownloadClient http;
        const auto metadata = http.getText(LauncherConfig::JavaRuntimeMetadataUrl,stopToken);
        if (!metadata.success)
        {
            return {false, false, L"Não foi possível consultar o Java 17: " +
                                  metadata.errorMessage,metadata.failure};
        }

        const auto package = JavaRuntimePackage::parse(metadata.content);
        if (!package)
        {
            return {false, false, L"A resposta do provedor do Java é inválida.",InstallationFailure::Configuration};
        }

        report(InstallationActivity::Downloading, L"Baixando Java 17 portátil...");
        const auto download = http.downloadFile(
            package->url, downloadPath, stopToken,
            [&](std::uint64_t received, std::uint64_t total)
            {
                const auto expected = total > 0 ? total : package->size;
                InstallationProgress value;
                value.activity=InstallationActivity::Downloading;value.message=L"Baixando Java 17 portátil...";
                value.receivedBytes=received;value.totalFiles=1;
                if(expected) {value.totalBytes=expected;value.percent=std::min(99,static_cast<int>(static_cast<double>(received)*100/static_cast<double>(expected)));}
                if(progress) progress(value);
            });
        if (!download.success)
        {
            return {false, false, download.errorMessage,download.failure};
        }

        report(InstallationActivity::Verifying, L"Verificando a integridade do Java...");
        if (!package->matchesFile(downloadPath,stopToken))
        {
            std::filesystem::remove(downloadPath);
            return {false, false, L"O SHA-256 do Java baixado não confere.",InstallationFailure::Integrity};
        }

        report(InstallationActivity::Extracting, L"Extraindo o Java portátil...");
        const auto installation = PortableJavaInstaller::install(
            downloadPath, dataPath, stopToken);
        std::error_code ignored;
        std::filesystem::remove(downloadPath, ignored);
        if (!installation.success)
        {
            return {false, false, installation.message,InstallationFailure::Process};
        }

        if(progress) progress({InstallationStage::Java,InstallationActivity::Publishing,InstallationStepState::Complete,L"Java 17 portátil instalado."});
        return {true, true, installation.message};
    }
    catch (const std::exception& error)
    {
        std::error_code ignored;
        std::filesystem::remove(downloadPath, ignored);
        return {false, false, L"Falha ao preparar o Java: " + utf8ToWide(error.what()),InstallationFailure::Disk};
    }
}

std::filesystem::path JavaRuntimeService::dataDirectory() const
{
    const auto appData = readEnvironmentVariable(L"APPDATA");
    return appData.empty()
        ? std::filesystem::current_path() / LauncherConfig::DataDirectoryName
        : std::filesystem::path(appData) / LauncherConfig::DataDirectoryName;
}
