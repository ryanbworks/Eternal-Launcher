#include "config/LauncherSettings.h"
#include "services/MinecraftLaunchCommandBuilder.h"
#include "services/MinecraftLauncherService.h"
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
    int failures=0;
    void check(bool success,const char* message) {
        if(!success) {std::cerr << message << '\n';++failures;}
    }
    std::string read(const std::filesystem::path& path) {
        std::ifstream input(path,std::ios::binary);
        return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    }
    void put(const std::filesystem::path& path,const std::string& value) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path,std::ios::binary) << value;
    }
}
int main() {
    const auto fixture=std::filesystem::temp_directory_path() /
        (L"eternal-settings-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+
        std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(fixture);
    // Only this process sees a private APPDATA fixture; user preferences are untouched.
    SetEnvironmentVariableW(L"APPDATA",(fixture/L"appdata").c_str());
    const auto store=LauncherSettingsStore::dataDirectory();
    std::wstring warning,error;
    const auto defaults=LauncherSettingsStore::load(store,&warning);
    check(defaults.maximumMemoryMegabytes==4096 && defaults.minimizeOnLaunch && warning.empty(),
        "missing settings must use defaults with minimize enabled without warning");
    LauncherSettings s;
    s.minimumMemoryMegabytes=1024;s.maximumMemoryMegabytes=6144;
    s.windowWidth=1920;s.windowHeight=1080;s.fullscreen=true;s.minimizeOnLaunch=true;
    s.gameDirectory=fixture/L"Mundos de Jo\u00e3o";
    s.javaExecutable=fixture/L"Java 17"/L"bin"/L"javaw.exe";
    put(s.javaExecutable,"");put(s.javaExecutable.parent_path()/L"java.exe","");
    put(s.javaExecutable.parent_path().parent_path()/L"release","JAVA_VERSION=\"17.0.12\"\n");
    check(LauncherSettingsStore::save(store,s,error),"valid settings must save");
    const auto stored=read(LauncherSettingsStore::filePath(store));
    const auto loaded=LauncherSettingsStore::load(store,&warning);
    check(warning.empty() && loaded.minimumMemoryMegabytes==1024 && loaded.maximumMemoryMegabytes==6144 &&
        loaded.windowWidth==1920 && loaded.windowHeight==1080 && loaded.fullscreen && loaded.minimizeOnLaunch &&
        loaded.gameDirectory==s.gameDirectory && loaded.javaExecutable==s.javaExecutable,"all settings must survive UTF-8 JSON round trip");
    s.minimumMemoryMegabytes=8192;
    check(!LauncherSettingsStore::save(store,s,error) && !error.empty(),"reversed RAM must be rejected");
    check(read(LauncherSettingsStore::filePath(store))==stored,"invalid save must preserve previous settings");
    s.minimumMemoryMegabytes=1024;
    const auto javaRelease=s.javaExecutable.parent_path().parent_path()/L"release";
    put(javaRelease,"JAVA_VERSION=\"170.0.1\"\n");
    check(!LauncherSettingsStore::validate(s,error),"Java 170 must not be accepted as Java 17");
    MinecraftLauncherService custom(s);
    check(custom.validateInstallation().issue==MinecraftLaunchIssue::InvalidConfiguration,"custom Java failure must not silently fall back");
    put(javaRelease,"JAVA_VERSION=\"17\"\n");
    check(LauncherSettingsStore::validate(s,error),"Java 17 without minor version is supported");
    std::filesystem::remove(s.javaExecutable.parent_path()/L"java.exe");
    check(!LauncherSettingsStore::validate(s,error),"javaw requires console java sibling for Forge");
    put(s.javaExecutable.parent_path()/L"java.exe","");
    const auto original=s.gameDirectory;
    s.gameDirectory=fixture/L"unrelated";
    put(s.gameDirectory/L"world.dat","keep me");
    check(!LauncherSettingsStore::validate(s,error),"unrelated nonempty folder must be rejected");
    check(read(s.gameDirectory/L"world.dat")=="keep me","validation must leave folder contents untouched");
    put(s.gameDirectory/L"eternal-instance.json","{\"launcher\":\"Eternal\"}");
    check(LauncherSettingsStore::validate(s,error),"marked Eternal instance can be reused");
    s.gameDirectory=store/L"runtime";
    check(!LauncherSettingsStore::validate(s,error,false),"private runtime path must be rejected even on load");
    auto upper=(store/L"RUNTIME").wstring();
    for(auto& c:upper) c=static_cast<wchar_t>(towupper(c));
    s.gameDirectory=upper;
    check(!LauncherSettingsStore::validate(s,error,false),"private folder protection must be case insensitive");
    s.gameDirectory=store.parent_path();
    check(!LauncherSettingsStore::validate(s,error,false),"ancestors of launcher data must be rejected");
    s.gameDirectory=fixture/L".minecraft";
    check(!LauncherSettingsStore::validate(s,error,false),"default Minecraft installation must be protected");
    s.gameDirectory=original;
    const MinecraftLaunchCommandBuilder builder;
    auto command=builder.build(MinecraftIdentity{L"Steve"},s.javaExecutable,s.gameDirectory,s);
    check(command.find(L"-Xms1024M")!=command.npos && command.find(L"-Xmx6144M")!=command.npos &&
        command.find(L"--width 1920 --height 1080 --fullscreen")!=command.npos,"launch command must include custom memory resolution and fullscreen");
    check(command.find(L"\""+s.gameDirectory.wstring()+L"\"")!=command.npos,"Unicode game path with spaces must be quoted");
    s.fullscreen=false;
    command=builder.build(MinecraftIdentity{L"Steve"},s.javaExecutable,s.gameDirectory,s);
    check(command.find(L"--fullscreen")==command.npos,"windowed launch must omit fullscreen argument");
    s.minimizeOnLaunch=false;
    check(LauncherSettingsStore::save(store,s,error) && !LauncherSettingsStore::load(store,&warning).minimizeOnLaunch && warning.empty(),
        "explicitly disabled minimize setting must survive saving and reopening");
    const auto broken=LauncherSettingsStore::filePath(store);
    put(broken,"{\"schemaVersion\":1}");
    check(LauncherSettingsStore::load(store,&warning).minimizeOnLaunch && warning.empty(),
        "settings without minimize preference must inherit the enabled default");
    for(const auto invalid:{"{broken", "{\"maximumMemoryMegabytes\":-1}","{\"fullscreen\":\"true\"}",
        "{\"schemaVersion\":2}","{\"windowWidth\":800.5}","{\"javaExecutable\":\"relative.exe\"}"}) {
        put(broken,invalid);
        const auto result=LauncherSettingsStore::load(store,&warning);
        check(!warning.empty() && result.maximumMemoryMegabytes==4096 && result.javaExecutable.empty(),"malformed settings must load safe defaults and explain why");
        check(read(broken)==invalid,"loading invalid preferences must preserve the original file");
    }
    std::filesystem::remove_all(fixture);
    std::cout << (failures ? "Failed settings checks: " : "Settings checks passed: ") << failures << '\n';
    return failures ? 1 : 0;
}
