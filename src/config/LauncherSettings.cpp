#include "config/LauncherSettings.h"
#include "utils/JsonValue.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string_view>

namespace
{
    std::string toUtf8(std::wstring_view value)
    {
        if (value.empty()) return {};
        const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (count <= 0) return {};
        std::string result(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            result.data(), count, nullptr, nullptr);
        return result;
    }
    std::wstring fromUtf8(std::string_view value)
    {
        if (value.empty()) return {};
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), nullptr, 0);
        if (count <= 0) return {};
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), count);
        return result;
    }
    std::string escape(std::wstring_view value)
    {
        std::ostringstream out;
        out << '"';
        for (const unsigned char c : toUtf8(value)) {
            if (c == '"' || c == '\\') out << '\\' << c;
            else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
            else out << c;
        }
        out << '"';
        return out.str();
    }
    bool validPathText(const std::filesystem::path& path)
    {
        const auto text = path.wstring();
        return text.size() <= 4096 && std::none_of(text.begin(),text.end(),[](wchar_t c) {
            return c < 32 || c == L'"' || c == L'*' || c == L'?' || c == L'|' || c == L'<' || c == L'>';
        });
    }
}

std::filesystem::path LauncherSettingsStore::dataDirectory()
{
    std::array<wchar_t,32768> value{};
    const auto size = GetEnvironmentVariableW(L"APPDATA",value.data(),static_cast<DWORD>(value.size()));
    return size > 0 && size < value.size()
        ? std::filesystem::path(value.data()) / LauncherConfig::DataDirectoryName
        : std::filesystem::current_path() / LauncherConfig::DataDirectoryName;
}
std::filesystem::path LauncherSettingsStore::filePath(const std::filesystem::path& directory)
{
    return directory / L"settings.json";
}
std::filesystem::path LauncherSettingsStore::effectiveGameDirectory(const LauncherSettings& settings)
{
    return settings.gameDirectory.empty() ? dataDirectory() / LauncherConfig::GameDirectoryName : settings.gameDirectory;
}
unsigned long long LauncherSettingsStore::physicalMemoryMegabytes()
{
    MEMORYSTATUSEX memory{sizeof(MEMORYSTATUSEX)};
    return GlobalMemoryStatusEx(&memory) ? memory.ullTotalPhys / (1024ULL*1024ULL) : 0;
}
bool LauncherSettingsStore::isJava17Executable(const std::filesystem::path& executable)
{
    std::error_code error;
    if (!executable.is_absolute() || !std::filesystem::is_regular_file(executable,error)) return false;
    const auto filename=executable.filename().wstring();
    if (_wcsicmp(filename.c_str(),L"java.exe") != 0 && _wcsicmp(filename.c_str(),L"javaw.exe") != 0) return false;
    if (!std::filesystem::is_regular_file(executable.parent_path()/L"java.exe",error)) return false;
    std::ifstream input(executable.parent_path().parent_path()/L"release");
    std::string line;
    while (std::getline(input,line)) {
        // A prefix alone would also accept Java 170.
        if (line.starts_with("JAVA_VERSION=\"17.") || line.starts_with("JAVA_VERSION=\"17\"")) return true;
    }
    return false;
}
bool LauncherSettingsStore::validate(const LauncherSettings& s,std::wstring& error,bool checkPaths)
{
    error.clear();
    if (s.minimumMemoryMegabytes < 512 || s.maximumMemoryMegabytes > 65536 ||
        s.maximumMemoryMegabytes < s.minimumMemoryMegabytes) {
        error=L"Use RAM entre 512 e 65536 MiB; a RAM inicial não pode superar a máxima."; return false;
    }
    if (s.windowWidth < 640 || s.windowWidth > 7680 || s.windowHeight < 360 || s.windowHeight > 4320) {
        error=L"Use largura entre 640 e 7680 e altura entre 360 e 4320 pixels."; return false;
    }
    for (const auto& path : {s.javaExecutable,s.gameDirectory}) if (!path.empty() &&
        (!path.is_absolute() || !validPathText(path))) {
        error=L"Informe um caminho absoluto válido, sem aspas ou caracteres de controle."; return false;
    }
    if (!s.gameDirectory.empty()) {
        std::error_code ec;
        const auto folder=std::filesystem::weakly_canonical(s.gameDirectory,ec);
        if (ec) { error=L"Não foi possível resolver a pasta escolhida."; return false; }
        const auto data=std::filesystem::weakly_canonical(dataDirectory(),ec);
        const auto equal=[](const std::filesystem::path& left,const std::filesystem::path& right) {
            return _wcsicmp(left.c_str(),right.c_str())==0;
        };
        const auto within=[&](const std::filesystem::path& child,const std::filesystem::path& ancestor) {
            auto c=child.begin();
            for(auto a=ancestor.begin();a!=ancestor.end();++a,++c)
                if(c==child.end() || !equal(*c,*a)) return false;
            return true;
        };
        if (ec || equal(folder,folder.root_path()) || within(data,folder) ||
            _wcsicmp(folder.filename().c_str(),L".minecraft")==0) {
            error=L"Escolha uma pasta exclusiva para o Eternal, diferente da raiz do disco ou de .minecraft."; return false;
        }
        if (within(folder,data) && !equal(folder,data / LauncherConfig::GameDirectoryName)) {
            error=L"Essa pasta é reservada aos dados do launcher. Escolha uma pasta exclusiva do jogo."; return false;
        }
        if (checkPaths && std::filesystem::exists(folder,ec)) {
            if (!std::filesystem::is_directory(folder,ec) ||
                (!std::filesystem::is_empty(folder,ec) &&
                 !std::filesystem::is_regular_file(folder/L"eternal-classpath.txt",ec) &&
                 !std::filesystem::is_regular_file(folder/L"eternal-instance.json",ec))) {
                error=L"A pasta deve estar vazia ou conter uma instância do Eternal. Isso protege arquivos de outras instalações."; return false;
            }
            if (ec) { error=L"Não foi possível acessar a pasta escolhida."; return false; }
        }
    }
    if (checkPaths && !s.javaExecutable.empty() && !isJava17Executable(s.javaExecutable)) {
        error=L"Selecione java.exe ou javaw.exe de uma instalação Java 17 com arquivo release válido."; return false;
    }
    return true;
}
LauncherSettings LauncherSettingsStore::load(const std::filesystem::path& directory,std::wstring* warning)
{
    if (warning) warning->clear();
    LauncherSettings settings;
    const auto path=filePath(directory);
    std::error_code error;
    if (!std::filesystem::exists(path,error) && !error) return settings;
    const auto bytes=std::filesystem::file_size(path,error);
    const auto invalid=[&]() {
        if (warning) *warning=L"Não foi possível ler as preferências. Os padrões foram carregados; o arquivo foi preservado.";
        return LauncherSettings{};
    };
    if (error || bytes > 65536) return invalid();
    std::ifstream input(path,std::ios::binary);
    if (!input) return invalid();
    const std::string text{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    const auto doc=JsonValue::parse(text);
    if (!doc || !doc->objectValue()) return invalid();
    const auto integer=[&](const char* key,int& target) {
        const auto* field=doc->find(key); if (!field) return true;
        const double value=field->numberValue(-1);
        if (!std::isfinite(value) || value != std::floor(value) || value < 0 || value > 1000000) return false;
        target=static_cast<int>(value); return true;
    };
    const auto boolean=[&](const char* key,bool& target) {
        const auto* field=doc->find(key); if (!field) return true;
        if (field->boolValue(true) != field->boolValue(false)) return false;
        target=field->boolValue(); return true;
    };
    const auto pathValue=[&](const char* key,std::filesystem::path& target) {
        const auto* field=doc->find(key); if (!field) return true;
        const auto* raw=field->stringValue(); if (!raw) return false;
        const auto wide=fromUtf8(*raw);
        if ((!raw->empty() && wide.empty()) || wide.find(L'\0') != std::wstring::npos) return false;
        target=wide; return true;
    };
    int version=1;
    if (!integer("schemaVersion",version) || version != 1 ||
        !integer("minimumMemoryMegabytes",settings.minimumMemoryMegabytes) ||
        !integer("maximumMemoryMegabytes",settings.maximumMemoryMegabytes) ||
        !integer("windowWidth",settings.windowWidth) || !integer("windowHeight",settings.windowHeight) ||
        !boolean("fullscreen",settings.fullscreen) || !boolean("minimizeOnLaunch",settings.minimizeOnLaunch) ||
        !pathValue("javaExecutable",settings.javaExecutable) || !pathValue("gameDirectory",settings.gameDirectory)) return invalid();
    std::wstring validation;
    return validate(settings,validation,false) ? settings : invalid();
}
bool LauncherSettingsStore::save(const std::filesystem::path& directory,const LauncherSettings& s,std::wstring& error)
{
    if (!validate(s,error)) return false;
    std::error_code ec;
    std::filesystem::create_directories(directory,ec);
    if (ec) { error=L"Não foi possível criar a pasta de preferências."; return false; }
    const auto destination=filePath(directory);
    const auto temporary=std::filesystem::path(destination.wstring()+L".tmp");
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        out << "{\n  \"schemaVersion\": 1,\n  \"minimumMemoryMegabytes\": " << s.minimumMemoryMegabytes
            << ",\n  \"maximumMemoryMegabytes\": " << s.maximumMemoryMegabytes
            << ",\n  \"windowWidth\": " << s.windowWidth << ",\n  \"windowHeight\": " << s.windowHeight
            << ",\n  \"fullscreen\": " << (s.fullscreen ? "true" : "false")
            << ",\n  \"minimizeOnLaunch\": " << (s.minimizeOnLaunch ? "true" : "false")
            << ",\n  \"javaExecutable\": " << escape(s.javaExecutable.wstring())
            << ",\n  \"gameDirectory\": " << escape(s.gameDirectory.wstring()) << "\n}\n";
        out.flush();
        if (!out) { error=L"Não foi possível gravar as preferências. O valor anterior foi preservado."; return false; }
    }
    if (!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        error=L"Não foi possível substituir o arquivo de preferências. Tente novamente."; return false;
    }
    return true;
}
