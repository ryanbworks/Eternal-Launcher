#include "services/ModInventory.h"
#include "services/NewsFeed.h"
#include "services/PlaytimeStore.h"

#include <windows.h>

#include <array>
#include <fstream>
#include <iostream>

namespace {
    int failures=0;
    void check(bool okay,const char* message) {if(!okay) {std::cerr << "FAIL: " << message << '\n';++failures;}}
    void write(const std::filesystem::path& path,const char* content) {std::ofstream output(path,std::ios::binary);output << content;}

    void playtime(const std::filesystem::path& root) {
        check(PlaytimeStore::formatDuration(30)==L"menos de 1 min","short sessions");
        check(PlaytimeStore::formatDuration(42*60)==L"42 min","minutes");
        check(PlaytimeStore::formatDuration(3*3600)==L"3 h","whole hours");
        check(PlaytimeStore::formatDuration(4*3600+12*60)==L"4 h 12 min","hours and minutes");
        const std::int64_t now=1'800'000'000;
        check(PlaytimeStore::formatLastPlayed(0,now)==L"Ainda não jogou","never played");
        check(PlaytimeStore::formatLastPlayed(now-30,now)==L"Agora há pouco","just now");
        check(PlaytimeStore::formatLastPlayed(now-3*3600,now)==L"Há 3 horas","hours ago");
        check(PlaytimeStore::formatLastPlayed(now-30*3600,now)==L"Ontem","yesterday");

        const auto directory=root/L"playtime";
        check(PlaytimeStore::load(directory).totalSeconds==0,"missing file starts at zero");
        PlaySession session;
        const auto start=PlaySession::Clock::now();
        session.start({600,60,1},start,now);
        check(session.active(),"session starts");
        const auto record=session.snapshot(start+std::chrono::seconds(125),now+125);
        check(record.totalSeconds==725 && record.lastSessionSeconds==125 && record.lastPlayed==now+125,
            "snapshot adds the running session to the stored total");
        check(PlaytimeStore::save(directory,record),"atomic save");
        check(PlaytimeStore::save(directory,record),"save replaces an existing file");
        const auto loaded=PlaytimeStore::load(directory);
        check(loaded.totalSeconds==725 && loaded.lastSessionSeconds==125 && loaded.lastPlayed==now+125,"saved record loads back");
        write(PlaytimeStore::filePath(directory),"{not json");
        check(PlaytimeStore::load(directory).totalSeconds==0,"a corrupt file never blocks the launcher");
        session.stop();
        check(!session.active() && session.elapsedSeconds(start+std::chrono::seconds(5))==0,"stopped session counts nothing");
    }

    void news() {
        const auto parsed=NewsFeed::parse(R"({"schemaVersion":1,"items":[
            {"category":"evento","title":"Noite de construção","date":"2026-10-05","body":"Tragam blocos.","url":"https://example.com/a"},
            {"title":"Sem link seguro","url":"http://example.com/b"},
            {"category":"aviso","body":"sem título"}]})");
        check(parsed.success && parsed.items.size()==2,"items without a title are skipped");
        if(parsed.items.size()==2) {
            check(parsed.items[0].category==L"EVENTO" && parsed.items[0].date==L"5 out 2026" && parsed.items[0].description==L"Tragam blocos.",
                "fields are normalized");
            check(parsed.items[1].url.empty() && parsed.items[1].category==L"AVISO","only https links are kept; category has a default");
        }
        check(!NewsFeed::parse("[]").success && !NewsFeed::parse("{\"items\":3}").success,"malformed feeds are rejected");
        check(NewsFeed::formatDate(L"2026-12-31")==L"31 dez 2026" && NewsFeed::formatDate(L"amanhã")==L"amanhã","date formatting");
        const auto notes=NewsFeed::releaseNotes();
        check(notes.local && !notes.title.empty() && notes.url.rfind(L"https://",0)==0,"release notes ship with the launcher");
    }

    void mods(const std::filesystem::path& root) {
        const auto directory=root/L"mods";
        std::filesystem::create_directories(directory);
        write(directory/L"create.jar","server");
        write(directory/L"old-server.jar","managed");
        write(directory/L".eternal-managed-mods","old-server.jar\n");
        write(directory/L"Journeymap.jar","player");
        write(directory/L"minimap.jar.disabled","player");
        write(directory/L"notes.txt","ignored");
        const std::wstring manifest=LR"({"files":[{"path":"mods/create.jar","size":6},{"path":"mods/missing.jar","size":9},{"path":"config/x.toml","size":1}]})";
        auto snapshot=ModInventory::scan(directory,manifest);
        check(snapshot.manifestKnown && snapshot.server.size()==2,"server mods come from the manifest's mods/ entries");
        if(snapshot.server.size()==2)
            check(snapshot.server[0].name==L"create.jar" && snapshot.server[0].installed && !snapshot.server[1].installed,
                "server mods report whether they are installed");
        check(snapshot.user.size()==2,"managed and server mods are not listed as player mods");
        if(snapshot.user.size()==2) {
            check(snapshot.user[0].name==L"Journeymap.jar" && snapshot.user[0].enabled,"player mods are sorted and enabled");
            check(snapshot.user[1].name==L"minimap.jar" && !snapshot.user[1].enabled,"disabled mods keep their base name");
            std::wstring error;
            check(ModInventory::setEnabled(snapshot.user[0],false,error) &&
                std::filesystem::exists(directory/L"Journeymap.jar.disabled"),"disabling renames to .jar.disabled");
            check(ModInventory::setEnabled(snapshot.user[1],true,error) && std::filesystem::exists(directory/L"minimap.jar"),
                "enabling restores the .jar name");
        }
        snapshot=ModInventory::scan(directory,L"");
        check(!snapshot.manifestKnown && snapshot.server.empty(),"without a manifest nothing is labelled as a server mod");
        write(root/L"extra.jar","x");write(root/L"readme.txt","x");
        std::wstring error;
        check(ModInventory::add(directory,root/L"extra.jar",error) && std::filesystem::exists(directory/L"extra.jar"),"adding copies the jar");
        check(!ModInventory::add(directory,root/L"extra.jar",error),"adding the same mod twice is refused");
        check(!ModInventory::add(directory,root/L"readme.txt",error),"only .jar files can be added");
        check(ModInventory::formatSize(512)==L"1 KB" && ModInventory::formatSize(5*1024*1024+512*1024)==L"5,5 MB","sizes use a decimal comma");
    }
}

int main()
{
    std::array<wchar_t,MAX_PATH> temporary{};
    GetTempPathW(static_cast<DWORD>(temporary.size()),temporary.data());
    const auto root=std::filesystem::path(temporary.data())/L"eternal-services-tests";
    std::error_code ignored;
    std::filesystem::remove_all(root,ignored);
    std::filesystem::create_directories(root);
    playtime(root);
    news();
    mods(root);
    std::filesystem::remove_all(root,ignored);
    std::cout << "Launcher services failures: " << failures << '\n';
    return failures ? 1 : 0;
}
