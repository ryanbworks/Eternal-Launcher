#include "services/PortableJavaInstaller.h"
#include "utils/ArchiveExtractor.h"

#include <fstream>
#include <optional>

namespace
{
    std::optional<std::filesystem::path> findExtractedRuntime(
        const std::filesystem::path& stagingDirectory)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(stagingDirectory))
        {
            if (entry.is_regular_file() && entry.path().filename() == L"javaw.exe" &&
                entry.path().parent_path().filename() == L"bin")
            {
                return entry.path().parent_path().parent_path();
            }
        }
        return std::nullopt;
    }
}

bool PortableJavaInstaller::isJava17Runtime(const std::filesystem::path& runtime)
{
    std::ifstream release(runtime / L"release");
    std::string line;
    while (std::getline(release, line))
    {
        if (line.starts_with("JAVA_VERSION=\"17"))
        {
            return std::filesystem::is_regular_file(runtime / L"bin" / L"javaw.exe") &&
                   std::filesystem::is_regular_file(runtime / L"bin" / L"java.exe");
        }
    }
    return false;
}

PortableJavaInstallResult PortableJavaInstaller::install(
    const std::filesystem::path& archive,
    const std::filesystem::path& dataDirectory,
    std::stop_token stopToken)
{
    const auto staging = dataDirectory / L"runtime-staging";
    const auto runtime = dataDirectory / L"runtime";
    const auto backup = dataDirectory / L"runtime-backup";
    std::error_code ignored;

    // Os nomes são fixos e filhos diretos da pasta privada do launcher.
    std::filesystem::remove_all(staging, ignored);
    std::filesystem::create_directories(staging);
    if (!ArchiveExtractor::extract(archive, staging, stopToken))
    {
        std::filesystem::remove_all(staging, ignored);
        return {false, L"Não foi possível extrair o Java portátil."};
    }

    const auto extracted = findExtractedRuntime(staging);
    if (!extracted || !isJava17Runtime(*extracted))
    {
        std::filesystem::remove_all(staging, ignored);
        return {false, L"O pacote extraído não contém um Java 17 válido."};
    }

    if(stopToken.stop_requested()) {
        std::filesystem::remove_all(staging,ignored);
        return {false,L"Instalação do Java cancelada."};
    }

    std::filesystem::remove_all(backup, ignored);
    if (std::filesystem::exists(runtime))
    {
        std::filesystem::rename(runtime, backup);
    }

    try
    {
        std::filesystem::rename(*extracted, runtime);
    }
    catch (...)
    {
        if (std::filesystem::exists(backup) && !std::filesystem::exists(runtime))
        {
            std::filesystem::rename(backup, runtime);
        }
        std::filesystem::remove_all(staging, ignored);
        throw;
    }

    std::filesystem::remove_all(backup, ignored);
    std::filesystem::remove_all(staging, ignored);
    return {true, L"Java 17 portátil instalado com sucesso."};
}
