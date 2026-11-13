#include "components/HomeView.h"
#include "services/MilestoneStore.h"
#include "services/ModInventory.h"
#include "services/NewsFeed.h"
#include "services/PlaytimeApi.h"
#include "services/PlaytimeStore.h"
#include "services/ScreenshotLibrary.h"
#include "services/SkinLookup.h"

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

    void leaderboard() {
        const auto parsed=PlaytimeApi::parseLeaderboard(R"({"players":[
            {"rank":1,"name":"Aurora_","totalSeconds":7200,"online":true},
            {"rank":2,"name":"<script>","totalSeconds":3600,"online":false},
            {"rank":3,"name":"Steve","totalSeconds":60}],
            "you":{"rank":3,"totalSeconds":60,"lastPlayed":5}})");
        check(parsed.success && parsed.players.size()==2,"names that are not valid nicknames are dropped");
        if(parsed.players.size()==2) {
            check(parsed.players[0].name==L"Aurora_" && parsed.players[0].rank==1 && parsed.players[0].totalSeconds==7200 && parsed.players[0].online,
                "leaderboard fields are read");
            check(!parsed.players[1].online,"a missing online flag means offline");
        }
        check(parsed.hasYou && parsed.yourRank==3 && parsed.yourSeconds==60,"the player's own position is read");
        check(!PlaytimeApi::parseLeaderboard(R"({"players":[],"you":null})").hasYou,"an unknown player has no position");
        check(!PlaytimeApi::parseLeaderboard("{oops").success && !PlaytimeApi::parseLeaderboard("{\"players\":3}").success,"malformed leaderboards are rejected");
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
        if(!parsed.items.empty()) check(parsed.items[0].isoDate==L"2026-10-05","the published ISO date is kept");
    }

    void newsReadState(const std::filesystem::path& root) {
        check(NewsFeed::ageInDays(L"2026-11-01",L"2026-11-13")==12 && NewsFeed::ageInDays(L"2025-12-31",L"2026-01-01")==1,"age in days");
        check(NewsFeed::ageInDays(L"",L"2026-11-13")==-1 && NewsFeed::ageInDays(L"2026-02-30",L"2026-11-13")==-1,"invalid dates have no age");
        NewsItem recent{L"EVENTO",L"Festa",L"",L"",L"",false,L"2026-11-10"};
        NewsItem old{L"AVISO",L"Antigo",L"",L"",L"",false,L"2026-08-01"};
        NewsItem undated{L"AVISO",L"Sem data",L"",L"",L"",false,L""};
        auto notes=NewsFeed::releaseNotes();
        std::vector<NewsItem> items{recent,old,undated,notes};
        const auto directory=root/L"news";
        NewsReadState state;
        state.load(directory);
        check(!state.exists(),"no state file before the first save");
        state.apply(items,L"2026-11-13");
        check(items[0].unread && !items[1].unread && items[2].unread && items[3].unread,
            "recent, undated and release notes are new; old server news never is");
        state.markSeen(recent);
        state.markDismissed(undated);
        check(state.save(),"read state saves");
        NewsReadState reloaded;
        reloaded.load(directory);
        check(reloaded.exists() && reloaded.seen(recent) && !reloaded.seen(undated) && reloaded.dismissed(undated) && !reloaded.dismissed(recent),
            "seen and dismissed survive a restart separately");
        reloaded.apply(items,L"2026-11-13");
        check(!items[0].unread && items[2].unread,"seen items are no longer new; dismissed ones stay unread");
        check(NewsFeed::key(notes)!=NewsFeed::key(recent),"release notes have their own key");
    }

    void screenshots(const std::filesystem::path& root) {
        Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
        if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok) {check(false,"GDI+ starts");return;}
        {
            UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
            std::vector<BYTE> bytes(size);auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());
            Gdiplus::GetImageEncoders(count,size,encoders);
            CLSID png{};for(UINT i=0;i<count;++i) if(std::wstring_view(encoders[i].MimeType)==L"image/png") png=encoders[i].Clsid;
            const auto modpack=root/L"shots"/L"modpack",vanilla=root/L"shots"/L"vanilla";
            std::filesystem::create_directories(modpack);std::filesystem::create_directories(vanilla);
            Gdiplus::Bitmap picture(640,360,PixelFormat32bppARGB);
            check(picture.Save((modpack/L"2026-11-10_20.00.00.png").c_str(),&png)==Gdiplus::Ok,"fixture picture saves");
            check(picture.Save((vanilla/L"2026-11-12_20.00.00.PNG").c_str(),&png)==Gdiplus::Ok,"fixture picture saves");
            write(modpack/L"notes.txt","not a picture");
            std::filesystem::last_write_time(modpack/L"2026-11-10_20.00.00.png",std::filesystem::file_time_type::clock::now()-std::chrono::hours(48));
            const auto entries=ScreenshotLibrary::scan(modpack,vanilla);
            check(entries.size()==2,"only pictures are listed, from both games");
            if(entries.size()==2) {
                check(entries[0].vanilla && !entries[1].vanilla && entries[0].modified>entries[1].modified,"newest first, game remembered");
                const auto thumb=ScreenshotLibrary::load(entries[1].path,416,234,true);
                check(thumb.bitmap && thumb.bitmap->GetWidth()==416 && thumb.bitmap->GetHeight()==234 && thumb.width==640 && thumb.height==360,
                    "thumbnails fill their box and report the original size");
                const auto large=ScreenshotLibrary::load(entries[1].path,320,320,false);
                check(large.bitmap && large.bitmap->GetWidth()==320 && large.bitmap->GetHeight()==180 && large.large,"the viewer image keeps its proportions");
                const auto original=ScreenshotLibrary::load(entries[1].path,1920,1080,false);
                check(original.bitmap && original.bitmap->GetWidth()==640,"pictures are never enlarged");
            }
            check(!ScreenshotLibrary::load(modpack/L"notes.txt",100,100,true).bitmap,"a file that is not a picture loads nothing");
            check(ScreenshotLibrary::scan(root/L"missing",{}).empty(),"a missing folder is just empty");
            check(ScreenshotLibrary::formatDate(0)==L"Data desconhecida","unknown dates");
        }
        Gdiplus::GdiplusShutdown(token);
    }

    void rankTiers() {
        check(RankTier::level(0)==1 && RankTier::level(24*3600+3599)==1,"level 1 below 25 h");
        check(RankTier::level(25*3600)==2 && RankTier::level(75*3600)==3 && RankTier::level(150*3600)==4 && RankTier::level(300*3600)==5,
            "each milestone raises the level");
        check(RankTier::level(5000ULL*3600)==RankTier::Levels,"the level is capped");
        check(std::wstring(RankTier::name(3))==L"Ouro" && std::wstring(RankTier::name(9))==L"Netherite","levels have names");
        check(RankTier::startHours(1)==0 && RankTier::startHours(4)==150,"where each level starts");
        check(std::abs(RankTier::progress(50*3600)-.5F)<.01F && RankTier::progress(400*3600)==1,"progress toward the next level");
    }

    void milestones(const std::filesystem::path& root) {
        const auto directory=root/L"milestones";
        MilestoneStore store;
        store.load(directory);
        check(store.celebrated(L"Steve")==-1,"an unknown player has no celebrated level");
        store.setCelebrated(L"Steve",3);
        store.setCelebrated(L"Aurora_",1);
        check(store.save(),"milestones save");
        MilestoneStore reloaded;
        reloaded.load(directory);
        check(reloaded.celebrated(L"steve")==3 && reloaded.celebrated(L"AURORA_")==1,"celebrated levels survive, case-insensitive");
        write(directory/L"milestones.txt","Steve\t4\nbroken line\n\t2\nLuna\tx\n");
        reloaded.load(directory);
        check(reloaded.celebrated(L"Steve")==4 && reloaded.celebrated(L"Luna")==-1,"malformed lines are ignored");
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

    std::string base64(std::string_view text) {
        static constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;std::uint32_t buffer=0;int bits=0;
        for(const unsigned char c:text) {buffer=(buffer<<8)|c;bits+=8;while(bits>=6){bits-=6;out.push_back(alphabet[(buffer>>bits)&63]);}}
        if(bits) out.push_back(alphabet[(buffer<<(6-bits))&63]);
        while(out.size()%4) out.push_back('=');
        return out;
    }
    std::string sessionProfile(std::string_view textures) {
        return R"({"id":"069a79f444e94726a5befca90e38aaf5","name":"Notch","properties":[{"name":"textures","value":")"+base64(textures)+R"("}]})";
    }

    void skins() {
        check(SkinLookup::parseProfileId(R"({"id":"069a79f444e94726a5befca90e38aaf5","name":"Notch"})")==std::string("069a79f444e94726a5befca90e38aaf5"),
            "profile id is read from the name lookup");
        check(!SkinLookup::parseProfileId(R"({"id":"../../evil","name":"x"})") && !SkinLookup::parseProfileId("not json"),
            "anything but a 32-digit hex id is refused");
        check(SkinLookup::base64Decode(base64("eternal launcher"))=="eternal launcher","base64 round trip");

        const auto url=SkinLookup::parseSkinUrl(sessionProfile(R"({"textures":{"SKIN":{"url":"http://textures.minecraft.net/texture/1a4af718455d4aab528e7a61f86fa25e6a369d1768dcb13f7df319a713eb810b"}}})"));
        check(url && *url==L"https://textures.minecraft.net/texture/1a4af718455d4aab528e7a61f86fa25e6a369d1768dcb13f7df319a713eb810b",
            "skin URL comes from the textures property and is upgraded to HTTPS");
        check(!SkinLookup::parseSkinUrl(sessionProfile(R"({"textures":{"SKIN":{"url":"http://evil.example/texture/abc"}}})")),
            "skins from other hosts are refused");
        check(!SkinLookup::parseSkinUrl(sessionProfile(R"({"textures":{"SKIN":{"url":"http://textures.minecraft.net/texture/../x"}}})")),
            "texture paths must be a plain hash");
        check(!SkinLookup::parseSkinUrl(sessionProfile(R"({"textures":{}})")),"accounts without a custom skin have no URL");

        std::vector<std::uint32_t> skin(64*64,0);
        for(int y=8;y<16;++y) for(int x=8;x<16;++x) skin[static_cast<std::size_t>(y*64+x)]=0xFFAA0000U;
        auto face=SkinLookup::composeFace(skin.data(),64,64,64);
        check(face.pixels[0]==0xFFAA0000U && face.pixels[63]==0xFFAA0000U,"face comes from the 8,8 region");
        skin[static_cast<std::size_t>(9*64+41)]=0xFF0000FFU;
        face=SkinLookup::composeFace(skin.data(),64,64,64);
        check(face.pixels[9]==0xFF0000FFU && face.pixels[0]==0xFFAA0000U,"opaque hat pixels cover the face");
        for(int y=8;y<16;++y) for(int x=40;x<48;++x) skin[static_cast<std::size_t>(y*64+x)]=0xFF000000U;
        face=SkinLookup::composeFace(skin.data(),64,32,64);
        check(face.pixels[9]==0xFFAA0000U,"a flat legacy hat fill is ignored");
        check(SkinLookup::composeFace(skin.data(),16,16,16).pixels==SkinLookup::defaultFace().pixels,"undersized skins use the default face");
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
    leaderboard();
    news();
    newsReadState(root);
    rankTiers();
    milestones(root);
    screenshots(root);
    mods(root);
    skins();
    std::filesystem::remove_all(root,ignored);
    std::cout << "Launcher services failures: " << failures << '\n';
    return failures ? 1 : 0;
}
