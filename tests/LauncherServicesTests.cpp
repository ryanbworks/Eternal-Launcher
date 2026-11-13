#include "components/HomeView.h"
#include "components/SkinRenderer.h"
#include "services/EternalAccount.h"
#include <wincrypt.h>
#include <shlwapi.h>
#include "services/GalleryApi.h"
#include "services/MilestoneStore.h"
#include "services/ModpackChangelog.h"
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

    void gallery(const std::filesystem::path& root) {
        const auto list=GalleryApi::parseList(R"({"photos":[
            {"id":"0123456789abcdef","name":"Steve","created":1800000000,"width":1920,"height":1080},
            {"id":"../../etc/passwd","name":"Steve"},
            {"id":"0123456789abcdee","name":"<script>"},
            {"id":"0123456789ABCDEF","name":"Aurora_"}]})");
        check(list.success && list.photos.size()==1,"only well-formed ids and nicknames are kept");
        if(!list.photos.empty()) check(list.photos[0].name==L"Steve" && list.photos[0].width==1920 && list.photos[0].created==1800000000,"photo fields are read");
        check(!GalleryApi::parseList("{oops").success && !GalleryApi::parseList(R"({"photos":3})").success,"malformed lists are rejected");
        const auto skins=GalleryApi::parseSkins(R"json({"skins":[
            {"id":"0123456789abcdef","name":"Steve","title":"Cavaleiro (fino)","slim":true,"created":5},
            {"id":"0123456789abcdee","name":"Steve","title":""},
            {"id":"nope","name":"Steve","title":"x"},
            {"id":"0123456789abcded","name":"a b","title":"x"}]})json");
        check(skins.success && skins.skins.size()==1 && skins.skins[0].title==L"Cavaleiro (fino)" && skins.skins[0].slim,
              "community skins keep only well-formed entries");
        check(!GalleryApi::parseSkins(R"({"skins":"x"})").success,"a malformed skin list is rejected");
        const auto catalog=GalleryApi::parseSkins(R"({"skins":[],"featured":[{"id":"0123456789abcdef","name":"Technoblade","title":"Technoblade"}]})");
        check(catalog.success && catalog.skins.empty() && catalog.featured.size()==1 && catalog.featured[0].name==L"Technoblade",
              "the Populares catalog comes apart from the shared skins");
        check(GalleryApi::parseSkins(R"({"skins":[]})").success,"servers without a catalog still list");
        check(GalleryApi::skinTitle(L"  Mago<>do  Fogo?! ")==L"Mago do Fogo !" && GalleryApi::skinTitle(L"%%%")==L"Skin" &&
              GalleryApi::skinTitle(L"Ação") == L"Ação" && GalleryApi::skinTitle(std::wstring(40,L'a')).size()==32,
              "library names become titles the server accepts");
        const auto body=GalleryApi::uploadBody("abc","defgh");
        check(body.size()==12 && body[0]==0 && body[3]==3 && body.substr(4,3)=="abc" && body.substr(7)=="defgh","upload body framing");

        Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
        if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok) {check(false,"GDI+ starts");return;}
        {
            Gdiplus::Bitmap picture(320,180,PixelFormat32bppARGB);
            {Gdiplus::Graphics g(&picture);Gdiplus::SolidBrush red(Gdiplus::Color(255,200,40,40));g.FillRectangle(&red,0,0,320,180);}
            const auto jpeg=GalleryApi::encodeJpeg(picture,85);
            check(jpeg.size()>100 && static_cast<unsigned char>(jpeg[0])==0xFF && static_cast<unsigned char>(jpeg[1])==0xD8,"JPEG encoding");
            const auto decoded=GalleryApi::decode(jpeg);
            check(decoded && decoded->GetWidth()==320 && decoded->GetHeight()==180,"JPEG decodes back");
            check(!GalleryApi::decode("not a picture"),"garbage does not decode");

            GallerySharedStore store;
            store.load(root/L"gallery");
            store.add({L"0123456789abcdef","token-1",GallerySharedStore::sourceKey(L"C:\\x\\shot.png",42)});
            check(store.save(),"shared store saves");
            GallerySharedStore reloaded;
            reloaded.load(root/L"gallery");
            check(reloaded.byId(L"0123456789abcdef") && reloaded.byId(L"0123456789abcdef")->token=="token-1" &&
                  reloaded.bySource(L"shot.png|42"),"shared pictures are remembered by id and by source");
            reloaded.erase(L"0123456789abcdef");
            check(!reloaded.byId(L"0123456789abcdef"),"a removed picture is forgotten");

            // Against a real gallery server, when one is given (see server/test_gallery_api.py).
            std::array<wchar_t,256> url{};
            if(GetEnvironmentVariableW(L"ETERNAL_GALLERY_TEST_URL",url.data(),static_cast<DWORD>(url.size()))) {
                CLSID pngEncoder{};
                UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
                std::vector<BYTE> bytes(size);auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());
                Gdiplus::GetImageEncoders(count,size,encoders);
                for(UINT i=0;i<count;++i) if(std::wstring_view(encoders[i].MimeType)==L"image/png") pngEncoder=encoders[i].Clsid;
                Gdiplus::Bitmap shot(2560,1440,PixelFormat32bppARGB);
                {Gdiplus::Graphics g(&shot);Gdiplus::SolidBrush blue(Gdiplus::Color(255,40,80,200));g.FillRectangle(&blue,0,0,2560,1440);}
                const auto file=root/L"gallery"/L"2026-11-13_20.00.00.png";
                check(shot.Save(file.c_str(),&pngEncoder)==Gdiplus::Ok,"integration screenshot saves");
                const auto uploaded=GalleryApi::upload(url.data(),file,L"Steve");
                check(uploaded.success && uploaded.id.size()==16 && !uploaded.deleteToken.empty(),"a screenshot uploads to the gallery");
                const auto listed=GalleryApi::fetchList(url.data());
                const bool found=std::any_of(listed.photos.begin(),listed.photos.end(),[&](const GalleryPhoto& p) {
                    return p.id==uploaded.id && p.name==L"Steve" && p.width==1920 && p.height==1080;});
                check(listed.success && found,"the upload is listed, scaled to 1920 x 1080");
                const auto thumb=GalleryApi::fetchImage(url.data(),uploaded.id,false);
                const auto full=GalleryApi::fetchImage(url.data(),uploaded.id,true);
                check(thumb && thumb->GetWidth()==416 && full && full->GetWidth()==1920,"thumbnail and picture download and decode");
                std::wstring error;
                check(GalleryApi::report(url.data(),uploaded.id,error),"a picture can be reported");
                check(!GalleryApi::remove(url.data(),uploaded.id,"wrong-token",error),"a wrong token cannot remove");
                check(GalleryApi::remove(url.data(),uploaded.id,uploaded.deleteToken,error),"the uploader removes the picture");
                check(!GalleryApi::fetchImage(url.data(),uploaded.id,false),"a removed picture is gone");

                // Community skins: share, list with textures, report, remove.
                Gdiplus::Bitmap texture(64,64,PixelFormat32bppARGB);
                {Gdiplus::Graphics g(&texture);Gdiplus::SolidBrush green(Gdiplus::Color(255,40,180,90));g.FillRectangle(&green,0,0,64,64);}
                const auto skinFile=root/L"gallery"/L"skin.png";
                check(texture.Save(skinFile.c_str(),&pngEncoder)==Gdiplus::Ok,"integration skin saves");
                std::ifstream skinInput(skinFile,std::ios::binary);
                const std::string png((std::istreambuf_iterator<char>(skinInput)),std::istreambuf_iterator<char>());
                const auto shared=GalleryApi::shareSkin(url.data(),png,L"Steve",L"Mago do Fogo (ação)",true);
                check(shared.success && shared.id.size()==16 && !shared.deleteToken.empty(),"a skin is shared");
                const auto community=GalleryApi::fetchSkins(url.data(),root/L"skin-cache");
                const bool listedSkin=std::any_of(community.skins.begin(),community.skins.end(),[&](const CommunitySkin& s) {
                    return s.id==shared.id && s.title==L"Mago do Fogo (ação)" && s.slim && s.png==png;});
                check(community.success && listedSkin,"the shared skin is listed with its title, model and texture");
                check(!GalleryApi::shareSkin(url.data(),"not a png",L"Steve",L"x",false).success,"only PNG skins are shared");
                check(GalleryApi::reportSkin(url.data(),shared.id,error),"a skin can be reported");
                check(!GalleryApi::removeSkin(url.data(),shared.id,"wrong-token",error),"a wrong token cannot remove a skin");
                check(GalleryApi::removeSkin(url.data(),shared.id,shared.deleteToken,error),"the owner removes the skin");
                const auto after=GalleryApi::fetchSkins(url.data(),root/L"skin-cache");
                check(std::none_of(after.skins.begin(),after.skins.end(),[&](const CommunitySkin& s) {return s.id==shared.id;}),"a removed skin is gone");
                std::cout << "gallery integration: ran against ETERNAL_GALLERY_TEST_URL\n";
            }
        }
        Gdiplus::GdiplusShutdown(token);
    }

    void eternalAccount(const std::filesystem::path& root) {
        const auto ok=EternalAccount::parseAuthentication(R"({"accessToken":"abc.def","clientToken":"c1",
            "selectedProfile":{"id":"0123456789ABCDEF0123456789abcdef","name":"Steve"}})","fallback");
        check(ok.success && ok.session.name==L"Steve" && ok.session.uuid==L"0123456789abcdef0123456789abcdef" &&
              ok.session.accessToken=="abc.def" && ok.session.clientToken=="c1","authentication responses are read, UUIDs lowercased");
        const auto dashed=EternalAccount::parseAuthentication(R"({"accessToken":"t","selectedProfile":{"id":"01234567-89ab-cdef-0123-456789abcdef","name":"Ana"}})","mine");
        check(dashed.success && dashed.session.uuid.size()==32 && dashed.session.clientToken=="mine","dashed UUIDs and a missing client token");
        check(!EternalAccount::parseAuthentication(R"({"accessToken":"t","availableProfiles":[]})","x").success,
            "an account without a player is refused with a message");
        check(!EternalAccount::parseAuthentication("{oops","x").success,"garbage is refused");

        // A textures property: base64 of {"textures":{"SKIN":{"url":...,"metadata":{"model":"slim"}}}}.
        const std::string textures=R"({"textures":{"SKIN":{"url":"https://accounts.test/drasl/texture/skin/a.png","metadata":{"model":"slim"}}}})";
        DWORD size=0;CryptBinaryToStringA(reinterpret_cast<const BYTE*>(textures.data()),static_cast<DWORD>(textures.size()),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&size);
        std::string encoded(size,'\0');
        CryptBinaryToStringA(reinterpret_cast<const BYTE*>(textures.data()),static_cast<DWORD>(textures.size()),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,encoded.data(),&size);
        encoded.resize(size);
        std::wstring url;
        const auto skin=EternalAccount::parseProfileTextures(R"({"id":"x","name":"Steve","properties":[{"name":"textures","value":")"+encoded+R"("}]})",url);
        check(skin && skin->custom && skin->slim && url==L"https://accounts.test/drasl/texture/skin/a.png","profile textures give the skin URL and model");
        const auto plain=EternalAccount::parseProfileTextures(R"({"id":"x","name":"Steve","properties":[]})",url);
        check(plain && !plain->custom && url.empty(),"no textures means the default skin");

        const auto profiles=EternalAccount::parseProfiles(R"([{"name":"zNippw","id":"5E24CF74F47C3875855A33090E036445"},{"name":"x","id":"bad"},{"id":"5e24cf74f47c3875855a33090e036445"}])");
        check(profiles.size()==1 && profiles[0].name==L"zNippw" && profiles[0].uuid==L"5e24cf74f47c3875855a33090e036445",
              "profile lookups keep valid names and lowercase UUIDs");
        check(EternalAccount::parseProfiles("{}").empty(),"a lookup that is not a list gives nothing");

        const auto body=EternalAccount::multipartSkin("B","PNGDATA",true);
        check(body.find("name=\"variant\"\r\n\r\nslim\r\n")!=std::string::npos && body.find("filename=\"skin.png\"")!=std::string::npos &&
              body.find("PNGDATA\r\n--B--")!=std::string::npos,"multipart skin upload");
        check(EternalAccount::agentArgument(L"C:\\x\\a.jar",L"https://accounts.test/")==L"C:\\x\\a.jar=https://accounts.test/authlib-injector",
            "javaagent argument");

        // PNG headers: 64 x 64 passes, other sizes and non-PNGs do not.
        const auto pngHeader=[](unsigned w,unsigned h){
            std::string png("\x89PNG\r\n\x1a\n\0\0\0\x0dIHDR",16);
            for(int s=24;s>=0;s-=8) png.push_back(static_cast<char>((w>>s)&0xFF));
            for(int s=24;s>=0;s-=8) png.push_back(static_cast<char>((h>>s)&0xFF));
            return png+std::string(20,'\0');
        };
        std::wstring error;
        check(EternalAccount::validSkinPng(pngHeader(64,64),error) && EternalAccount::validSkinPng(pngHeader(64,32),error),"64 x 64 and 64 x 32 skins pass");
        check(!EternalAccount::validSkinPng(pngHeader(128,128),error) && error.find(L"128")!=std::wstring::npos,"other sizes are refused with their size");
        check(!EternalAccount::validSkinPng("GIF89a....................",error),"non-PNG files are refused");

        // The session store keeps the token encrypted and reads it back.
        const auto directory=root/L"account";
        EternalSession session{L"Steve",L"0123456789abcdef0123456789abcdef","secret-token","client"};
        check(EternalSessionStore::save(directory,session),"session saves");
        std::ifstream raw(directory/L"account.dat",std::ios::binary);
        const std::string onDisk((std::istreambuf_iterator<char>(raw)),std::istreambuf_iterator<char>());
        raw.close();
        check(onDisk.find("secret-token")==std::string::npos && onDisk.find("Steve")!=std::string::npos,"the token is never written in clear");
        const auto loaded=EternalSessionStore::load(directory);
        check(loaded && loaded->accessToken=="secret-token" && loaded->name==L"Steve" && loaded->clientToken=="client","session loads back");
        EternalSessionStore::clear(directory);
        check(!EternalSessionStore::load(directory),"signing out removes the session");
        check(EternalAccount::newClientToken().size()==32 && EternalAccount::newClientToken()!=EternalAccount::newClientToken(),"client tokens are random");

        // Against a real Drasl, when one is given: ETERNAL_ACCOUNTS_TEST_URL, _USER, _PASSWORD.
        std::array<wchar_t,256> accounts{},user{},password{};
        if(!GetEnvironmentVariableW(L"ETERNAL_ACCOUNTS_TEST_URL",accounts.data(),256) || !GetEnvironmentVariableW(L"ETERNAL_ACCOUNTS_TEST_USER",user.data(),256) ||
           !GetEnvironmentVariableW(L"ETERNAL_ACCOUNTS_TEST_PASSWORD",password.data(),256)) return;
        const auto wrong=EternalAccount::authenticate(accounts.data(),user.data(),L"definitely-wrong","client");
        check(!wrong.success && wrong.rejected && wrong.errorMessage==L"Usuário ou senha incorretos.","a wrong password is rejected in Portuguese");
        const auto signedIn=EternalAccount::authenticate(accounts.data(),user.data(),password.data(),EternalAccount::newClientToken());
        check(signedIn.success && signedIn.session.valid(),"the account signs in");
        if(!signedIn.success) return;
        // Minecraft's offline UUID for the player's name: MD5("OfflinePlayer:<name>") as a version 3 UUID.
        {
            std::string seed="OfflinePlayer:";
            for(const wchar_t c:signedIn.session.name) seed.push_back(static_cast<char>(c)); // nicknames are ASCII
            BCRYPT_ALG_HANDLE algorithm=nullptr;BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_MD5_ALGORITHM,nullptr,0);
            std::array<unsigned char,16> digest{};
            BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(seed.data())),static_cast<ULONG>(seed.size()),digest.data(),16);
            BCryptCloseAlgorithmProvider(algorithm,0);
            digest[6]=static_cast<unsigned char>((digest[6]&0x0F)|0x30);digest[8]=static_cast<unsigned char>((digest[8]&0x3F)|0x80);
            std::wstring offline;for(const auto b:digest) {wchar_t pair[3];swprintf_s(pair,L"%02x",b);offline+=pair;}
            check(signedIn.session.uuid==offline,"the account keeps the offline-mode UUID, so the world data carries over");
        }
        const auto valid=EternalAccount::ensureValid(accounts.data(),signedIn.session);
        check(valid.success && valid.session.accessToken==signedIn.session.accessToken,"a fresh token validates");

        Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&input,nullptr);
        {
            UINT count=0,encoderBytes=0;Gdiplus::GetImageEncodersSize(&count,&encoderBytes);
            std::vector<BYTE> bytes(encoderBytes);auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());
            Gdiplus::GetImageEncoders(count,encoderBytes,encoders);
            CLSID png{};for(UINT i=0;i<count;++i) if(std::wstring_view(encoders[i].MimeType)==L"image/png") png=encoders[i].Clsid;
            Gdiplus::Bitmap texture(64,64,PixelFormat32bppARGB);
            {Gdiplus::Graphics g(&texture);Gdiplus::SolidBrush blue(Gdiplus::Color(255,40,90,200));g.FillRectangle(&blue,0,0,64,64);}
            IStream* stream=SHCreateMemStream(nullptr,0);
            texture.Save(stream,&png,nullptr);
            STATSTG stat{};stream->Stat(&stat,STATFLAG_NONAME);
            std::string skinPng(static_cast<std::size_t>(stat.cbSize.QuadPart),'\0');
            const LARGE_INTEGER start{};ULONG read=0;stream->Seek(start,STREAM_SEEK_SET,nullptr);
            stream->Read(skinPng.data(),static_cast<ULONG>(skinPng.size()),&read);stream->Release();
            std::wstring skinError;
            check(EternalAccount::uploadSkin(accounts.data(),signedIn.session,skinPng,true,skinError),"a skin uploads with the account's token");
            const auto current=EternalAccount::fetchSkin(accounts.data(),signedIn.session.uuid);
            check(current && current->custom && current->slim && !current->png.empty(),"the uploaded skin and its slim model come back");
            if(current && current->custom) {
                const auto decoded=SkinRenderer::decode(current->png);
                check(decoded && (decoded->at(20,20)&0xFFFFFF)==0x285AC8,"the published texture is the one sent");
            }
            check(EternalAccount::resetSkin(accounts.data(),signedIn.session,skinError),"the skin resets to the default");
            const auto reset=EternalAccount::fetchSkin(accounts.data(),signedIn.session.uuid);
            check(reset && !reset->custom,"after a reset the player has the default skin");
        }
        Gdiplus::GdiplusShutdown(token);
        EternalAccount::signOut(accounts.data(),signedIn.session);
        const auto revoked=EternalAccount::ensureValid(accounts.data(),signedIn.session);
        check(!revoked.success && revoked.rejected,"signing out revokes the token on the server");
        std::cout << "conta eternal integration: ran against ETERNAL_ACCOUNTS_TEST_URL\n";
    }

    // Registration and first access against the real services, when asked to:
    // ETERNAL_ACCOUNTS_TEST_URL, ETERNAL_FILES_TEST_URL, ETERNAL_REGISTER_TEST_NAME and
    // ETERNAL_FIRST_ACCESS_TEST_NAME (a disposable account already in the pending list).
    void eternalOnboarding() {
        std::array<wchar_t,256> accounts{},files{},newName{},firstName{};
        if(!GetEnvironmentVariableW(L"ETERNAL_ACCOUNTS_TEST_URL",accounts.data(),256) || !GetEnvironmentVariableW(L"ETERNAL_FILES_TEST_URL",files.data(),256) ||
           !GetEnvironmentVariableW(L"ETERNAL_REGISTER_TEST_NAME",newName.data(),256) || !GetEnvironmentVariableW(L"ETERNAL_FIRST_ACCESS_TEST_NAME",firstName.data(),256)) return;
        std::wstring error;
        check(EternalAccount::registerAccount(accounts.data(),newName.data(),L"senha-de-teste-123",error),"a new account is created from the launcher");
        check(!EternalAccount::registerAccount(accounts.data(),newName.data(),L"outra-senha-123",error) && error==L"Esse nick já tem conta. Se é seu, use Entrar.",
            "a taken nickname is refused in Portuguese");
        check(!EternalAccount::registerAccount(accounts.data(),L"Outro_Nome",L"curta",error) && error.find(L"8")!=std::wstring::npos,"short passwords are refused");
        const auto registered=EternalAccount::authenticate(accounts.data(),newName.data(),L"senha-de-teste-123",EternalAccount::newClientToken());
        check(registered.success && registered.session.name==newName.data(),"the new account signs in");

        const auto stranger=EternalAccount::checkFirstAccess(files.data(),newName.data());
        check(stranger.known && !stranger.pending,"an account with its own password is not a first access");
        const auto pending=EternalAccount::checkFirstAccess(files.data(),firstName.data());
        check(pending.known && pending.pending,"a reserved account is a first access by nickname only");
        check(!EternalAccount::claimFirstAccess(files.data(),firstName.data(),L"curta",error),"first access needs 8 characters");
        check(EternalAccount::claimFirstAccess(files.data(),firstName.data(),L"minha-senha-nova",error),"first access sets the player's password");
        const auto claimed=EternalAccount::authenticate(accounts.data(),firstName.data(),L"minha-senha-nova",EternalAccount::newClientToken());
        check(claimed.success,"the player signs in with the new password");
        check(!EternalAccount::checkFirstAccess(files.data(),firstName.data()).pending,"after that the nickname is no longer a first access");
        check(!EternalAccount::claimFirstAccess(files.data(),firstName.data(),L"roubando-a-conta",error) && error.find(L"Entrar")!=std::wstring::npos,
            "nobody can claim it a second time");
        std::cout << "conta eternal onboarding: ran against the real services\n";
    }

    void skinRenderer() {
        Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;
        if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok) {check(false,"GDI+ starts");return;}
        {
            Gdiplus::Bitmap texture(64,64,PixelFormat32bppARGB);
            {Gdiplus::Graphics g(&texture);Gdiplus::SolidBrush red(Gdiplus::Color(255,200,0,0));g.FillRectangle(&red,0,0,64,64);}
            const auto png=GalleryApi::encodeJpeg(texture,90); // any encoded picture works for decode()
            const auto decoded=SkinRenderer::decode(png);
            check(decoded && decoded->width==64 && decoded->height==64 && (decoded->at(10,10)>>24)==0xFF,"skins decode to pixels");
            if(decoded) {
                check(!SkinRenderer::looksSlim(*decoded),"filled arm columns mean classic arms");
                auto slim=*decoded;
                for(int y=20;y<32;++y) {slim.pixels[static_cast<std::size_t>(y*64+54)]=0;slim.pixels[static_cast<std::size_t>(y*64+55)]=0;}
                check(SkinRenderer::looksSlim(slim),"empty arm columns mean slim arms");
                Gdiplus::Bitmap canvas(32,64,PixelFormat32bppARGB);
                {Gdiplus::Graphics g(&canvas);SkinRenderer::drawBody(g,*decoded,{0,0,32,64},false,false);}
                Gdiplus::Color body;canvas.GetPixel(16,30,&body);
                check(body.GetA()==255 && body.GetR()>150,"the body is drawn from the texture");
            }
            Gdiplus::Bitmap wrong(32,32,PixelFormat32bppARGB);
            check(!SkinRenderer::decode(GalleryApi::encodeJpeg(wrong,90)),"textures that are not 64 wide are refused");
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

    void modpackChangelog(const std::filesystem::path& root) {
        using namespace ModpackChangelog;
        check(modKey(L"create-1.20.1-0.5.1.f.jar")==L"create" && modKey(L"Xaeros_Minimap_24.2.0_Forge_1.20.jar")==L"xaeros_minimap" &&
              modKey(L"geckolib-forge-1.20.1-4.4.4.jar")==L"geckolib-forge" && modKey(L"curios-forge-5.9.1+1.20.1.jar")==L"curios-forge" &&
              modKey(L"modname-mc1.20.1-2.0.jar")==L"modname" && modKey(L"Noversion.jar")==L"noversion","mod keys drop the version");
        check(displayName(L"sophisticated_backpacks-1.20.1-3.20.5.jar")==L"Sophisticated Backpacks","readable names");
        check(version(L"create-1.20.1-0.5.1.f.jar")==L"0.5.1.f" && version(L"Noversion.jar").empty(),"versions come from the file name");
        check(version(L"jei-1.20.1-forge-15.3.0.4.jar")==L"15.3.0.4" && version(L"comforts-forge-6.4.0+1.20.1.jar")==L"6.4.0" &&
              version(L"supplementaries-1.20-2.8.10.jar")==L"2.8.10" && version(L"oldmap-1.0.jar")==L"1.0","the Minecraft version and loader are dropped");
        const auto changes=compare({L"create-1.20.1-0.5.1.jar",L"jei-15.3.jar",L"oldmap-1.0.jar"},
                                   {L"create-1.20.1-0.5.2.jar",L"jei-15.3.jar",L"aquaculture-2.5.jar"});
        check(changes.added.size()==1 && changes.added[0].name==L"Aquaculture","new mods are added");
        check(changes.removed.size()==1 && changes.removed[0].name==L"Oldmap","missing mods are removed");
        check(changes.updated.size()==1 && changes.updated[0].previousVersion==L"0.5.1" && changes.updated[0].version==L"0.5.2",
            "a new version of the same mod is an update");
        check(compare({L"a-1.jar"},{L"A-1.jar"}).empty(),"case changes alone are not changes");
        const auto directory=root/L"modpack";
        check(!load(directory).has_value(),"no snapshot before the first save");
        check(save(directory,{L"create-1.jar",L"jei-2.jar"}),"snapshot saves");
        const auto loaded=load(directory);
        check(loaded && loaded->size()==2 && (*loaded)[1]==L"jei-2.jar","snapshot loads back");
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
        check(SkinLookup::parseSlim(sessionProfile(R"({"textures":{"SKIN":{"url":"x","metadata":{"model":"slim"}}}})")) &&
              !SkinLookup::parseSlim(sessionProfile(R"({"textures":{"SKIN":{"url":"x"}}})")),"the slim model comes from the skin metadata");
        check(SkinLookup::fetchOriginal(L"a/b", {}).outcome==SkinLookupOutcome::NotFound &&
              SkinLookup::fetchOriginal(L"ab", {}).outcome==SkinLookupOutcome::NotFound,"invalid nicknames are not looked up");
        // Against Mojang, when asked: ETERNAL_MOJANG_TEST=1 (Notch has used a custom skin for years).
        std::array<wchar_t,8> mojang{};
        if(GetEnvironmentVariableW(L"ETERNAL_MOJANG_TEST",mojang.data(),static_cast<DWORD>(mojang.size()))) {
            const auto notch=SkinLookup::fetchOriginal(L"notch",{});
            check(notch.outcome==SkinLookupOutcome::Found && notch.custom && notch.name==L"Notch" && notch.png.size()>100,
                  "an original account's whole skin downloads, with the name as the account writes it");
            check(SkinLookup::fetchOriginal(L"zz_no_such_nick9",{}).outcome==SkinLookupOutcome::NotFound,"unknown nicknames are not found");
            std::cout << "mojang integration: ran\n";
        }

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
    modpackChangelog(root);
    screenshots(root);
    gallery(root);
    eternalAccount(root);
    eternalOnboarding();
    skinRenderer();
    mods(root);
    skins();
    std::filesystem::remove_all(root,ignored);
    std::cout << "Launcher services failures: " << failures << '\n';
    return failures ? 1 : 0;
}
