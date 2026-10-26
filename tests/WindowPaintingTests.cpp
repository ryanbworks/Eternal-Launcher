#include "pages/MainPage.h"
#include "components/Layout.h"
#include "constants/LauncherConstants.h"
#include "utils/DrawingUtils.h"
#include "services/MinecraftLaunchCommandBuilder.h"
#include "services/NewsFeed.h"
#include "services/PlaytimeStore.h"
#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

using namespace LauncherConstants;

namespace {
    int failures=0;
    void check(bool success,const char* description) {
        if (!success) { std::cerr << "FAIL: " << description << '\n'; ++failures; }
    }
    void validateChildren(HWND parent) {
        ValidateRect(parent,nullptr);
        EnumChildWindows(parent,[](HWND child,LPARAM)->BOOL { ValidateRect(child,nullptr); return TRUE; },0);
    }
    COLORREF brushColor(HBRUSH brush) {
        LOGBRUSH value{};
        return GetObjectW(brush,sizeof(value),&value) ? value.lbColor : CLR_INVALID;
    }
    bool shown(HWND window) { return window && (GetWindowLongPtrW(window,GWL_STYLE)&WS_VISIBLE)!=0; }
    CLSID pngEncoder() {
        UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
        std::vector<BYTE> storage(size);const auto encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
        Gdiplus::GetImageEncoders(count,size,encoders);CLSID png{};
        for(UINT i=0;i<count;++i) if(std::wstring_view(encoders[i].MimeType)==L"image/png") png=encoders[i].Clsid;
        return png;
    }
    void pump(int milliseconds) {
        const auto deadline=GetTickCount64()+static_cast<ULONGLONG>(milliseconds);
        while(GetTickCount64()<deadline) {
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&message);DispatchMessageW(&message);}
            Sleep(2);
        }
    }
}

class InstallationTestLauncher final : public IMinecraftLauncherService {
public:
    int launches=0;
    bool running=false;
    MinecraftLaunchResult validateInstallation() const override {return {true,L"Pronto"};}
    MinecraftLaunchResult launch(std::wstring_view) override {++launches;running=true;return {true,L"Iniciado",123};}
    MinecraftLaunchResult launch(const MinecraftIdentity&) override {++launches;running=true;return {true,L"Iniciado",123};}
    bool isGameRunning() const override {return running;}
    std::filesystem::path gameDirectory() const override {return {};}
};

class WindowPaintingTest {
public:
    // Hidden test-owned host built from the production window procedure and controls.
    static bool host(MainPage& page,const wchar_t* className,int width=1280,int height=800) {
        WNDCLASSW klass{};
        klass.hInstance=page.instance_;klass.lpszClassName=className;
        klass.lpfnWndProc=MainPage::windowProcedure;klass.hbrBackground=page.backgroundBrush_;
        check(RegisterClassW(&klass)!=0,"test-owned window registration");
        page.window_=CreateWindowExW(0,className,L"Eternal — teste",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            0,0,width,height,nullptr,nullptr,page.instance_,&page);
        if(!page.window_) {check(false,"hidden production window procedure");return false;}
        page.createControls();
        check(page.settingsPage_.create(page.window_,page.instance_),"settings native children");
        page.settingsPage_.show(false);
        SetWindowTextW(page.nicknameInput_,L"Steve");
        page.layoutControls();page.syncControls();
        return true;
    }
    static void release(MainPage& page,const wchar_t* className) {
        if(page.nicknameInput_ && IsWindow(page.nicknameInput_)) SetWindowTextW(page.nicknameInput_,L""); // never save a test nickname
        if(IsWindow(page.window_)) DestroyWindow(page.window_);
        UnregisterClassW(className,page.instance_);
    }
    static void resize(MainPage& page,int width,int height) {
        // WM_NCCALCSIZE removes the native frame, so the window size is the client size.
        SetWindowPos(page.window_,nullptr,0,0,width,height,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        page.layoutControls();page.redraw();
    }
    static HWND control(MainPage& page,int id) {const auto* entry=page.findControl(id);return entry ? entry->window : nullptr;}
    static RECT childRect(MainPage& page,HWND child) {
        RECT bounds{};GetWindowRect(child,&bounds);
        MapWindowPoints(nullptr,page.window_,reinterpret_cast<POINT*>(&bounds),2);return bounds;
    }
    static void tick(MainPage& page) {page.previousFrame_=std::chrono::steady_clock::now()-std::chrono::milliseconds(32);page.animate();}

    static void chromeAndNavigation(MainPage& page) {
        for(const int id:{NavHomeId,NavNewsId,NavModsId,NavSettingsId,NavHelpId,MinimizeButtonId,MaximizeButtonId,CloseButtonId})
            check(shown(control(page,id)),"window chrome and navigation are always visible");
        check(shown(control(page,PlayButtonId)) && shown(page.nicknameInput_) && !shown(control(page,ModsAddButtonId)),
            "home shows the player card only");

        // Native layout follows the uniform logical canvas.
        const float s=page.viewScale_;
        const auto play=childRect(page,control(page,PlayButtonId));
        check(play.left==static_cast<LONG>(std::floor(page.offsetX_+Layout::Play.X*s)) &&
              play.top==static_cast<LONG>(std::floor(page.offsetY_+Layout::Play.Y*s)),"play button sits on its scaled logical bounds");
        RECT client{};GetClientRect(page.window_,&client);
        const auto close=childRect(page,control(page,CloseButtonId));
        check(std::abs(close.right-client.right)<=1 && close.top==0,"close button is anchored to the top-right corner");
        const auto home=childRect(page,control(page,NavHomeId));
        check(home.left==static_cast<LONG>(std::floor(14*s)),"navigation rail is anchored to the left edge, not centered");

        // Custom frame: drag on the title bar, resize at the edges, content elsewhere.
        const auto hit=[&](int x,int y) {POINT p{x,y};ClientToScreen(page.window_,&p);
            return SendMessageW(page.window_,WM_NCHITTEST,0,MAKELPARAM(p.x,p.y));};
        check(hit(client.right/2,static_cast<int>(20*s))==HTCAPTION,"empty title bar drags the window");
        check(hit(client.right/2,client.bottom/2)==HTCLIENT,"content is client area");
        check(hit(client.right-1,client.bottom-1)==HTBOTTOMRIGHT,"bottom-right corner resizes");
        check(hit(0,client.bottom/2)==HTLEFT,"left edge resizes");
        RECT window{};GetWindowRect(page.window_,&window);
        check(window.right-window.left==client.right && window.bottom-window.top==client.bottom,"custom frame leaves no native caption");

        // Page switches show only the active page's controls and keep focus reachable.
        SetFocus(control(page,PlayButtonId));
        // A hidden CI desktop may refuse focus to an inactive window; only assert what was granted.
        const bool focusGranted=GetFocus()==control(page,PlayButtonId);
        page.showPage(LauncherPage::Mods);
        check(page.page_==LauncherPage::Mods && shown(control(page,ModsAddButtonId)) && !shown(control(page,PlayButtonId)) &&
              !shown(page.nicknameInput_),"mods page replaces the home controls");
        if(focusGranted) check(GetFocus()==control(page,NavModsId),"focus moves off a control that was hidden");
        else std::cout << "note: focus not granted to the test window; hidden-focus check skipped\n";
        SendMessageW(control(page,NavSettingsId),BM_CLICK,0,0);
        check(page.page_==LauncherPage::Settings && shown(GetDlgItem(page.window_,SettingsPage::Save)) &&
              shown(GetDlgItem(page.window_,SettingsPage::RamSlider)) && !shown(control(page,ModsAddButtonId)),"settings page shows its form");
        MSG escape{};escape.hwnd=page.window_;escape.message=WM_KEYDOWN;escape.wParam=VK_ESCAPE;
        check(page.handleKeyboard(escape) && page.page_==LauncherPage::Home && !shown(GetDlgItem(page.window_,SettingsPage::Save)),
            "Escape returns home and hides the settings form");
        SendMessageW(control(page,NavHelpId),BM_CLICK,0,0);
        check(page.page_==LauncherPage::Help && shown(control(page,HelpRepairButtonId)) && shown(control(page,HelpCopySupportButtonId)),
            "help page exposes its actions");
        SendMessageW(control(page,NavHomeId),BM_CLICK,0,0);
        check(page.page_==LauncherPage::Home,"home navigation");
        page.originalMode_=true;page.syncControls();
        check(!shown(page.nicknameInput_) && !shown(control(page,EditNicknameButtonId)),"original account hides the nickname field");
        page.originalMode_=false;page.syncControls();
    }

    static void newsAndMods(MainPage& page) {
        page.viewState_.news.clear();
        for(int i=0;i<6;++i) page.viewState_.news.push_back({L"AVISO",L"Notícia "+std::to_wstring(i),L"Texto",L"25 out 2026",
            i%2==0 ? L"https://example.com/"+std::to_wstring(i) : L"",false});
        page.syncControls();
        check(shown(control(page,NewsCardFirstId+2)),"home shows three news cards");
        SendMessageW(control(page,NewsCardFirstId+1),BM_CLICK,0,0);
        check(page.page_==LauncherPage::News && page.newsFirst_==1,"a news card opens the list at that item");
        const Gdiplus::PointF inside{600,400};
        page.scrollContent(-WHEEL_DELTA,inside);page.scrollContent(-WHEEL_DELTA*3,inside);
        check(page.newsFirst_==2,"news scrolling stops at the last page");
        check(shown(control(page,NewsLinkFirstId)) && !shown(control(page,NewsLinkFirstId+1)),"only items with a link show Abrir");
        page.scrollContent(WHEEL_DELTA*10,inside);
        check(page.newsFirst_==0,"news scrolling stops at the first item");

        // Mods: a temporary instance with one server mod and two player mods.
        const auto game=LauncherSettingsStore::dataDirectory()/L"mods-test-instance";
        std::error_code ignored;std::filesystem::remove_all(game,ignored);
        std::filesystem::create_directories(game/L"mods");
        {std::ofstream(game/L"mods"/L"server.jar")<<"server";std::ofstream(game/L"mods"/L"mine.jar")<<"mine";
         std::ofstream(game/L"mods"/L"old.jar.disabled")<<"old";std::ofstream(game/L"mods"/L".eternal-managed-mods")<<"server.jar\n";}
        page.settings_.gameDirectory=game;
        page.fileServerManifest_=L"{\"files\":[{\"path\":\"mods/server.jar\",\"size\":6},{\"path\":\"config/a.toml\",\"size\":1}]}";
        SendMessageW(control(page,NavModsId),BM_CLICK,0,0);
        check(page.mods_.manifestKnown && page.mods_.server.size()==1 && page.mods_.user.size()==2,"inventory separates server and player mods");
        const auto list=ModsView::userList();
        const auto mine=std::find_if(page.mods_.user.begin(),page.mods_.user.end(),[](const UserMod& mod){return mod.enabled;});
        check(mine!=page.mods_.user.end(),"enabled player mod found");
        if(mine!=page.mods_.user.end()) {
            const auto row=static_cast<float>(mine-page.mods_.user.begin());
            const Gdiplus::PointF toggle{list.GetRight()-82,list.Y+row*ModsView::UserRowHeight+30};
            check(ModsView::hitTest(page.modsModel(),toggle.X,toggle.Y).action==ModsView::Action::Toggle,"switch hit target");
            page.handleModsClick(toggle);
            check(std::filesystem::exists(game/L"mods"/L"mine.jar.disabled") && !std::filesystem::exists(game/L"mods"/L"mine.jar"),
                "switch disables a player mod by renaming it");
            page.handleModsClick(toggle);
            check(std::filesystem::exists(game/L"mods"/L"mine.jar"),"switch enables it again");
        }
        page.scrollContent(-WHEEL_DELTA,{list.X+10,list.Y+10});
        check(page.userScroll_==0,"short lists do not scroll");
        page.settings_.gameDirectory.clear();page.fileServerManifest_.clear();
        std::filesystem::remove_all(game,ignored);
        page.viewState_.news={NewsFeed::releaseNotes()};page.newsFirst_=0;
        SendMessageW(control(page,NavHomeId),BM_CLICK,0,0);
    }

    static void updatesAndPainting(MainPage& page,const LauncherViewState& initial) {
        page.updatingLauncher_=true;
        for(const auto phase:{LauncherUpdatePhase::Checking,LauncherUpdatePhase::Downloading,
            LauncherUpdatePhase::Applying,LauncherUpdatePhase::Restarting}) {
            page.applyLauncherUpdateProgress({phase,phase==LauncherUpdatePhase::Checking ? 0 : 47});
            check(!page.viewState_.playEnabled,"launcher update must block play");
            for(const int id:{PlayButtonId,OfflineButtonId,OriginalButtonId,PlayMenuButtonId,QuickRepairButtonId})
                check(!IsWindowEnabled(control(page,id)),"player actions must be disabled during self update");
            check(IsWindowEnabled(control(page,NavSettingsId)),"navigation stays available during self update");
        }
        page.updatingLauncher_=false;page.viewState_=initial;
        page.updateWarning_=L"A atualização falhou. A versão anterior foi restaurada.";
        page.syncControls();
        check(shown(control(page,UpdateDownloadButtonId)),"recovery must expose manual download");
        check(IsWindowEnabled(control(page,PlayButtonId)),"update failure must let the installed launcher continue");
        page.updateWarning_.clear();page.syncControls();
        check(!shown(control(page,UpdateDownloadButtonId)),"manual download must hide without a warning");

        const auto dc=CreateCompatibleDC(nullptr);
        const auto screen=GetDC(nullptr);
        const auto bitmap=CreateCompatibleBitmap(screen,560,100);
        ReleaseDC(nullptr,screen);
        const auto previous=SelectObject(dc,bitmap);
        const auto dark=CreateSolidBrush(RGB(13,14,20));
        const RECT bounds{0,0,560,100};
        const auto drawButton=[&](HWND button) {
            RECT child{};GetClientRect(button,&child);
            DRAWITEMSTRUCT item{};
            item.CtlType=ODT_BUTTON;item.CtlID=GetDlgCtrlID(button);item.hwndItem=button;
            item.hDC=dc;item.rcItem=child;item.itemAction=ODA_DRAWENTIRE;
            SendMessageW(page.window_,WM_DRAWITEM,item.CtlID,reinterpret_cast<LPARAM>(&item));
        };
        const auto buttonCheck=[&](HWND button,const char* description) {
            FillRect(dc,&bounds,dark);
            const auto erased=SendMessageW(button,WM_ERASEBKGND,reinterpret_cast<WPARAM>(dc),0);
            check(erased!=0 && GetPixel(dc,2,2)==RGB(13,14,20),description);
            const auto brush=reinterpret_cast<HBRUSH>(SendMessageW(page.window_,WM_CTLCOLORBTN,
                reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(button)));
            check(brushColor(brush)==RGB(13,14,20),"owner-drawn button fallback brush must be dark");
        };
        // Quick actions are ghost buttons: their hover fill is visible at the left edge.
        const auto quick=control(page,QuickFolderButtonId);
        buttonCheck(quick,"main button erasure must not expose a light background");
        drawButton(quick);
        const auto idle=GetPixel(dc,6,20);
        validateChildren(page.window_);
        page.motionEnabled_=false;
        SendMessageW(quick,WM_MOUSEMOVE,0,MAKELPARAM(4,4));
        check(!GetUpdateRect(page.nicknameInput_,nullptr,FALSE),"hover must leave the native text field paint untouched");
        drawButton(quick);
        check(GetPixel(dc,6,20)!=idle,"child must see updated hover even before parent WM_PAINT");
        SendMessageW(quick,WM_MOUSELEAVE,0,0);
        const auto minimize=control(page,MinimizeButtonId);
        drawButton(minimize);const auto captionIdle=GetPixel(dc,4,4);
        SendMessageW(minimize,WM_MOUSEMOVE,0,MAKELPARAM(4,4));drawButton(minimize);
        check(GetPixel(dc,4,4)!=captionIdle,"caption buttons highlight on hover");
        SendMessageW(minimize,WM_MOUSELEAVE,0,0);

        page.showPage(LauncherPage::Settings);
        page.redraw();
        const auto slider=GetDlgItem(page.window_,SettingsPage::RamSlider);
        LauncherSettings memory;
        page.settingsPage_.open(LauncherSettings{},{});
        check(SendMessageW(slider,TBM_GETPOS,0,0)==8,"default 4 GB must map to slider step 8");
        for (const int step:{2,3,4,9,13}) {
            SendMessageW(slider,TBM_SETPOS,FALSE,step);
            SendMessageW(page.window_,WM_HSCROLL,MAKEWPARAM(TB_THUMBTRACK,step),reinterpret_cast<LPARAM>(slider));
            check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==step*512,
                "slider notifications must collect 1, 1.5, 2, 4.5 and 6.5 GB without rounding");
            const auto command=MinecraftLaunchCommandBuilder{}.build(MinecraftIdentity{L"Steve"},L"C:/Java/bin/java.exe",
                L"C:/Eternal Test/minecraft",memory);
            check(command.find(L"-Xmx"+std::to_wstring(step*512)+L"M")!=command.npos,"half-step slider value must reach the JVM");
        }
        std::array<wchar_t,128> sliderName{};
        GetWindowTextW(slider,sliderName.data(),static_cast<int>(sliderName.size()));
        check(std::wstring_view(sliderName.data()).find(L"6,5 GB")!=std::wstring_view::npos,"selected value must use Portuguese decimal comma");
        SendMessageW(GetDlgItem(page.window_,SettingsPage::Ram4),BM_CLICK,0,0);
        check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==4096,"4 GB shortcut must update collected memory");
        SendMessageW(slider,WM_KEYDOWN,VK_RIGHT,0);
        check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==4608,"native right arrow must add half a GB");
        SendMessageW(slider,WM_KEYDOWN,VK_NEXT,0);
        check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==5632,"native Page Down must add one GB");
        for (const int preset:{SettingsPage::Ram6,SettingsPage::Ram8}) {
            SendMessageW(GetDlgItem(page.window_,preset),BM_CLICK,0,0);
            check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==(preset==SettingsPage::Ram6 ? 6144 : 8192),
                "6 and 8 GB shortcuts must synchronize fields");
        }
        SetWindowTextW(GetDlgItem(page.window_,SettingsPage::MaxRam),L"6400");
        check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==6400 && SendMessageW(slider,TBM_GETPOS,0,0)==13,
            "precise MiB input must remain exact and synchronize the nearest slider position");
        SetWindowTextW(GetDlgItem(page.window_,SettingsPage::MaxRam),L"32768");
        check(SendMessageW(slider,TBM_GETRANGEMAX,0,0)==64 && SendMessageW(slider,TBM_GETPOS,0,0)==64,
            "existing larger allocations must expand the slider range");
        page.viewState_.playEnabled=false;page.redraw();
        check(!IsWindowEnabled(slider) && !IsWindowEnabled(GetDlgItem(page.window_,SettingsPage::Ram4)) &&
              IsWindowEnabled(GetDlgItem(page.window_,SettingsPage::TabAdvanced)),"busy operation must disable editing but keep the tabs");
        SendMessageW(slider,TBM_SETPOS,FALSE,8);
        SendMessageW(page.window_,WM_HSCROLL,TB_THUMBTRACK,reinterpret_cast<LPARAM>(slider));
        page.viewState_.playEnabled=true;page.redraw();
        check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==32768,"busy form must ignore memory scroll notifications");
        SetWindowTextW(GetDlgItem(page.window_,SettingsPage::MinRam),L"8192");
        SendMessageW(GetDlgItem(page.window_,SettingsPage::Ram4),BM_CLICK,0,0);
        check(page.settingsPage_.collect(memory) && memory.minimumMemoryMegabytes==4096 && memory.maximumMemoryMegabytes==4096,
            "lowering maximum via preset must also lower a larger initial allocation");
        SendMessageW(GetDlgItem(page.window_,SettingsPage::Restore),BM_CLICK,0,0);
        check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==4096 && memory.minimizeOnLaunch &&
            SendMessageW(slider,TBM_GETRANGEMAX,0,0)==32,"restore must synchronize default memory range and the tray preference");
        buttonCheck(slider,"slider erasure must preserve dark background");
        FillRect(dc,&bounds,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SendMessageW(slider,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
        check(GetPixel(dc,1,1)!=RGB(255,255,255),"native slider painting must cover its background without a white flash");
        const auto save=GetDlgItem(page.window_,SettingsPage::Save);
        buttonCheck(save,"settings button erasure must not expose a light background");
        drawButton(save);
        const auto settingsIdle=GetPixel(dc,12,20);
        validateChildren(page.window_);
        SendMessageW(save,WM_MOUSEMOVE,0,MAKELPARAM(4,4));
        drawButton(save);
        check(GetPixel(dc,12,20)!=settingsIdle,"settings child must see updated hover even before parent WM_PAINT");
        for (const int id:{SettingsPage::MinRam,SettingsPage::MaxRam,SettingsPage::Width,SettingsPage::Height,
            SettingsPage::JavaPath,SettingsPage::GamePath}) {
            check(!GetUpdateRect(GetDlgItem(page.window_,id),nullptr,FALSE),"settings hover must not invalidate EDIT children");
        }
        const auto white=GetStockObject(WHITE_BRUSH);
        for(int frame=0;frame<24;++frame) {
            SendMessageW(save,frame%2 ? WM_MOUSELEAVE : WM_MOUSEMOVE,0,MAKELPARAM(4,4));
            FillRect(dc,&bounds,static_cast<HBRUSH>(white));
            drawButton(save);
            check(GetPixel(dc,1,1)!=RGB(255,255,255),"owner draw must cover every frame after hover changes");
        }
        SendMessageW(save,WM_MOUSELEAVE,0,0);
        page.motionEnabled_=true;
        page.showPage(LauncherPage::Home);
        SelectObject(dc,previous);DeleteObject(bitmap);DeleteObject(dark);DeleteDC(dc);
    }

    static void installationFlow(MainPage& page,const LauncherViewState& initial,InstallationTestLauncher& fake) {
        page.viewState_=initial;
        page.settings_.minimizeOnLaunch=false;
        page.installation_.begin();page.installationMailbox_.begin(page.installation_.attempt);
        page.viewState_.playEnabled=false;page.viewState_.progressVisible=true;
        page.redraw();
        const auto cancel=control(page,CancelInstallationButtonId);
        check(shown(cancel) && shown(control(page,InstallationLogButtonId)),"installation exposes native cancel and log controls");
        check(!shown(control(page,NewsAllButtonId)) && shown(control(page,PlayButtonId)),"installation replaces the left column only");
        check(!IsWindowEnabled(control(page,PlayButtonId)) && IsWindowEnabled(cancel),"preparation blocks play and allows cancellation");
        const auto attempt=page.installation_.attempt;
        SendMessageW(page.window_,WM_COMMAND,MAKEWPARAM(PlayButtonId,BN_CLICKED),reinterpret_cast<LPARAM>(control(page,PlayButtonId)));
        check(page.installation_.attempt==attempt,"repeated play cannot start another active attempt");
        SendMessageW(cancel,BM_CLICK,0,0);SendMessageW(cancel,BM_CLICK,0,0);
        check(page.installation_.cancelling && !IsWindowEnabled(cancel),"native cancel is idempotent and disables repeated clicks");
        page.installationMailbox_.finish(attempt,{InstallationOutcome::Success});page.consumeInstallationProgress();
        check(fake.launches==0 && page.installation_.result->outcome==InstallationOutcome::Cancelled,"cancel wins queued successful preparation");
        check(page.viewState_.playEnabled && page.viewState_.primaryActionLabel==L"CONTINUAR","cancelled installation can continue");
        page.installation_.begin();page.installationMailbox_.begin(page.installation_.attempt);
        page.viewState_.playEnabled=false;
        page.installationMailbox_.finish(attempt,{InstallationOutcome::Success});page.consumeInstallationProgress();
        check(fake.launches==0 && page.installation_.running,"old attempt result cannot launch a new attempt");

        // Repair runs the same preparation without starting the game.
        page.installation_={};page.viewState_=initial;page.redraw();
        page.repairOnly_=true;page.installation_.begin();page.installationMailbox_.begin(page.installation_.attempt);
        page.viewState_.playEnabled=false;
        page.installationMailbox_.finish(page.installation_.attempt,{InstallationOutcome::Success,InstallationFailure::None,L"Pronto",{},3});
        page.consumeInstallationProgress();
        check(fake.launches==0 && !page.installation_.visible && page.viewState_.playEnabled && !page.repairOnly_,
            "successful repair returns to the home screen without launching");

        page.installation_.begin();page.installationMailbox_.begin(page.installation_.attempt);
        page.viewState_.playEnabled=false;
        page.installationMailbox_.finish(page.installation_.attempt,{InstallationOutcome::Success,InstallationFailure::None,L"Pronto",{},3});
        page.consumeInstallationProgress();
        check(fake.launches==1 && !page.installation_.visible && page.viewState_.operation==LauncherOperation::MinecraftRunning,
            "successful preparation launches exactly once and restores the landscape");
        check(page.playSession_.active(),"launching starts counting play time");
        check(!IsWindowEnabled(control(page,QuickRepairButtonId)),"repair is unavailable while the game runs");
    }

    static void playtimeAndTray(MainPage& page,InstallationTestLauncher& fake) {
        const auto data=LauncherSettingsStore::dataDirectory();
        // Pretend the session started two minutes ago.
        const auto base=PlaytimeStore::load(data);
        page.playSession_.start(base,PlaySession::Clock::now()-std::chrono::seconds(125),static_cast<std::int64_t>(std::time(nullptr)));
        SendMessageW(page.window_,WM_TIMER,PlaytimeTimerId,0);
        auto saved=PlaytimeStore::load(data);
        check(saved.totalSeconds>=base.totalSeconds+125 && saved.lastPlayed>0,"the minute timer flushes play time to disk");
        const bool tray=page.trayIcon_.visible();
        if(!tray) std::cout << "note: notification area unavailable on this desktop; tray hiding not exercised\n";
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);
        if(tray) {
            SendMessageW(page.window_,WM_CLOSE,0,0);
            check(IsWindow(page.window_) && !IsWindowVisible(page.window_) && page.hiddenToTray_ && page.exitAfterGame_,
                "closing during a session hides to the tray instead of exiting");
            SendMessageW(page.window_,TrayIconMessage,0,MAKELPARAM(NIN_SELECT,1));
            check(IsWindowVisible(page.window_) && !page.hiddenToTray_ && !page.exitAfterGame_,"clicking the tray icon restores the window");
            page.hideToTray(false);
            check(!IsWindowVisible(page.window_) && !page.exitAfterGame_,"hiding on launch keeps the launcher after the game");
        }
        fake.running=false;
        page.pollGameProcess();
        saved=PlaytimeStore::load(data);
        check(!page.playSession_.active() && !page.trayIcon_.visible() && saved.totalSeconds>=base.totalSeconds+125,
            "game exit stops the session, saves the time and removes the tray icon");
        check(page.viewState_.playEnabled && page.viewState_.operationMessage.find(L"Sessão de 2 min")!=std::wstring::npos,
            "the finished session length is reported");
        if(tray) check(IsWindowVisible(page.window_),"the hidden window comes back when the game ends");
        ShowWindow(page.window_,SW_HIDE);
    }

    static void motion(MainPage& page,const LauncherViewState& initial) {
        Gdiplus::Bitmap source(32,32,PixelFormat32bppPARGB),destination(128,128,PixelFormat32bppPARGB);
        {Gdiplus::Graphics fill(&source);fill.Clear(Gdiplus::Color(255,100,50,200));}
        Gdiplus::Graphics cachedDraw(&destination);
        std::unique_ptr<Gdiplus::Bitmap> raster;
        DrawingUtils::drawCachedImage(cachedDraw,source,raster,{0,0,16,16},{0,0,32,32});
        check(raster && raster->GetWidth()==16 && raster->GetHeight()==16,"image cache is sized for the actual display");
        const auto firstRaster=raster.get();
        DrawingUtils::drawCachedImage(cachedDraw,source,raster,{0,0,16,16},{0,0,32,32});
        check(raster.get()==firstRaster,"unchanged animation frames reuse the prepared raster");

        page.installation_={};page.viewState_=initial;page.page_=LauncherPage::Home;page.pageFade_=1;
        page.portalEntrance_=1;page.portalBurst_=0;page.portalEnergy_=0;page.displayedProgress_=0;
        page.originalMode_=false;page.identitySelection_=0;
        page.motionEnabled_=true;page.ambientMotion_=false;
        for(auto& entry:page.controls_) {entry.hover=0;entry.hovered=false;}
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);page.redraw();page.prepareFrame();
        {
            Gdiplus::Bitmap presented(page.frame_->GetWidth(),page.frame_->GetHeight(),PixelFormat32bppPARGB);
            Gdiplus::Graphics output(&presented);
            const float dpiX=page.frame_->GetHorizontalResolution(),dpiY=page.frame_->GetVerticalResolution();
            page.frame_->SetResolution(72,72);
            const auto target=output.GetHDC();SendMessageW(page.window_,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(target),PRF_CLIENT);output.ReleaseHDC(target);
            bool identical=true;
            for(int y=20;y<80;y+=7) for(int x=32;x<500;x+=17) {
                Gdiplus::Color sourceColor,targetColor;page.frame_->GetPixel(x,y,&sourceColor);presented.GetPixel(x,y,&targetColor);
                identical &= sourceColor.GetValue()==targetColor.GetValue();
            }
            check(identical,"native presentation copies buffer pixels without a second DPI-dependent scale");
            page.frame_->SetResolution(dpiX,dpiY);
        }
        check(!page.animationTimer_,"a settled home without ambient motion keeps no timer");
        page.originalMode_=true;page.redraw();
        check(page.animationTimer_ && page.identitySelection_==0,"identity updates immediately while its indicator starts moving");
        tick(page);check(page.identitySelection_>0 && page.identitySelection_<1,"identity selection interpolates without snapping");
        page.originalMode_=false;page.redraw();
        for(int i=0;i<24;++i) tick(page);
        check(page.identitySelection_==0 && !page.animationTimer_,"interrupted selection reverses and settles without an idle timer");
        const auto hero=[&](float x,float y) {return MAKELPARAM(static_cast<int>(page.offsetX_+x*page.viewScale_),static_cast<int>(page.offsetY_+y*page.viewScale_));};
        SendMessageW(page.window_,WM_MOUSEMOVE,0,hero(500,200));
        check(page.portalHovered_ && page.animationTimer_,"pointer over the landscape activates portal energy");
        tick(page);check(page.portalEnergy_>0 && page.portalEnergy_<1,"portal activation interpolates");
        SendMessageW(page.window_,WM_MOUSELEAVE,0,0);
        for(int i=0;i<28;++i) tick(page);
        check(page.portalEnergy_==0 && !page.animationTimer_,"portal energy stops after the pointer leaves");
        const auto quick=control(page,QuickLogsButtonId);
        SendMessageW(quick,WM_MOUSEMOVE,0,MAKELPARAM(4,4));
        check(page.animationTimer_,"button hover eases in");
        tick(page);
        const auto* entry=page.findControl(QuickLogsButtonId);
        check(entry && entry->hover>0 && entry->hover<1,"hover interpolates");
        SendMessageW(quick,WM_MOUSELEAVE,0,0);
        for(int i=0;i<24;++i) tick(page);
        check(entry && entry->hover==0 && !page.animationTimer_,"hover settles and stops the timer");
        page.showPage(LauncherPage::News);
        check(page.pageFade_==0 && page.animationTimer_,"page switches fade in");
        tick(page);
        check(page.navIndicator_>Layout::navButton(0).Y && page.navIndicator_<Layout::navButton(1).Y,
            "the navigation selection slides toward the new page");
        for(int i=0;i<10;++i) tick(page);
        check(page.pageFade_==1 && !page.animationTimer_,"page fade completes and stops");
        check(page.navIndicator_==Layout::navButton(1).Y,"the navigation selection settles on the page");
        page.showPage(LauncherPage::Settings);for(int i=0;i<10;++i) tick(page);
        check(!page.animationTimer_,"settings settles after its entrance");
        SendMessageW(GetDlgItem(page.window_,SettingsPage::Fullscreen),BM_CLICK,0,0);
        check(page.animationTimer_,"a switch slides instead of jumping");
        for(int i=0;i<10;++i) tick(page);
        check(!page.animationTimer_,"the switch settles and stops the timer");
        page.settingsPage_.open(page.settings_,{});
        page.showPage(LauncherPage::Home);for(int i=0;i<10;++i) tick(page);
        SendMessageW(control(page,PlayButtonId),WM_MOUSEMOVE,0,MAKELPARAM(30,20));
        check(page.playSweep_==0 && page.animationTimer_,"hovering Play starts a light sweep");
        SendMessageW(control(page,PlayButtonId),WM_MOUSELEAVE,0,0);
        for(int i=0;i<30;++i) tick(page);
        check(page.playSweep_==1 && !page.animationTimer_,"the sweep finishes once and stops");
        page.portalEntrance_=0;page.redraw();
        check(page.animationTimer_,"visible portal entrance schedules frames");
        ShowWindow(page.window_,SW_HIDE);page.redraw();
        check(!page.animationTimer_,"hidden window suspends all animation timers");
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);page.redraw();
        for(int i=0;i<40;++i) tick(page);
        check(page.portalEntrance_==1 && !page.animationTimer_,"portal opens once and stops rendering when settled");
        ShowWindow(page.window_,SW_SHOWMINNOACTIVE);page.redraw();
        check(!page.animationTimer_,"minimized window suspends animation");
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);page.redraw();
        page.installation_.begin();page.viewState_.playEnabled=false;
        page.installation_.progress.activity=InstallationActivity::Downloading;page.installation_.progress.percent=80;page.redraw();tick(page);
        check(page.installationFill_>0 && page.installationFill_<80,"real transfer fill eases toward the current percentage");
        ++page.installation_.progress.transferId;page.redraw();
        check(page.installationFill_==0,"new transfer cannot inherit the previous animated fill");
        page.installation_.steps[0]=InstallationStepState::Complete;page.redraw();
        check(page.completionPulse_[0]==1,"stage completion triggers a confirmation pulse");
        for(int i=0;i<14;++i) tick(page);
        check(page.completionPulse_[0]==0,"completion pulse ends without changing the stage state");
        page.portalHovered_=true;page.portalBurst_=1;page.portalEnergy_=1;
        page.originalMode_=true;page.findControl(CancelInstallationButtonId)->hovered=true;page.motionEnabled_=false;page.redraw();
        check(page.identitySelection_==1 && page.installationFill_==80 && page.findControl(CancelInstallationButtonId)->hover==1 &&
            !page.animationTimer_,"reduced motion immediately preserves selection, progress and hover feedback");
        check(page.portalBurst_==0 && page.portalEnergy_==0,"reduced motion removes portal energy");
        page.showPage(LauncherPage::Settings);check(page.pageFade_==1 && !page.animationTimer_,"reduced motion switches pages without a fade");
        page.showPage(LauncherPage::Home);
        page.installation_={};page.viewState_=initial;page.originalMode_=false;page.motionEnabled_=true;page.redraw();
        ShowWindow(page.window_,SW_HIDE);
    }

    static void closeDuringPreparation(MainPage& page,InstallationTestLauncher& fake) {
        page.installation_.begin();page.installationMailbox_.begin(page.installation_.attempt);
        page.viewState_.playEnabled=false;page.viewState_.progressVisible=true;
        const auto closingAttempt=page.installation_.attempt;
        const int launches=fake.launches;
        page.installationThread_=std::jthread([&page,closingAttempt](std::stop_token stop) {
            while(!stop.stop_requested()) Sleep(5);
            Sleep(80); // Assert that WM_CLOSE keeps pumping during worker cleanup.
            page.installationMailbox_.finish(closingAttempt,{InstallationOutcome::Cancelled});
            PostMessageW(page.window_,InstallationProgressMessage,0,0);
        });
        SetWindowTextW(page.nicknameInput_,L""); // The test must never save a real nickname.
        const auto closeStart=GetTickCount64();SendMessageW(page.window_,WM_CLOSE,0,0);
        check(GetTickCount64()-closeStart<60 && IsWindow(page.window_),"closing during preparation defers destruction without blocking");
        const auto deadline=GetTickCount64()+2000;
        while(IsWindow(page.window_) && GetTickCount64()<deadline) pump(5);
        check(!IsWindow(page.window_) && fake.launches==launches,"close waits for cleanup and never launches pending installation");
    }

    static void run() {
        MainPage page(GetModuleHandleW(nullptr));
        if(!host(page,L"EternalPaintingRegressionHost")) return;
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);
        page.viewState_.playEnabled=true;
        page.viewState_.operation=LauncherOperation::ReadyToPlay;
        page.viewState_.primaryActionLabel=L"JOGAR";
        page.motionEnabled_=false;
        page.syncControls();
        const auto initial=page.viewState_;
        chromeAndNavigation(page);
        newsAndMods(page);
        updatesAndPainting(page,initial);
        auto fake=std::make_unique<InstallationTestLauncher>();auto& fakeLauncher=*fake;
        page.minecraftLauncherService_=std::move(fake);
        installationFlow(page,initial,fakeLauncher);
        playtimeAndTray(page,fakeLauncher);
        motion(page,initial);
        closeDuringPreparation(page,fakeLauncher);
        release(page,L"EternalPaintingRegressionHost");
    }

    static void benchmark() {
        MainPage page(GetModuleHandleW(nullptr));
        if(!host(page,L"EternalMotionBenchmarkHost")) return;
        page.motionEnabled_=true;page.viewState_.playEnabled=true;page.viewState_.operation=LauncherOperation::ReadyToPlay;
        const auto measure=[](const char* name,auto work) {
            std::vector<double> times;
            for(int i=0;i<48;++i) {
                const auto start=std::chrono::steady_clock::now();work(i);
                if(i>=8) times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            }
            std::sort(times.begin(),times.end());
            std::cout << name << " median_ms=" << times[times.size()/2] << " p95_ms=" << times[times.size()*95/100] << std::endl;
        };
        for(const auto& size:{std::pair{1280,800},std::pair{1024,640}}) {
            resize(page,size.first,size.second);
            measure("home",[&](int i) {page.particlePhase_=static_cast<float>(i)/48;page.frameDirty_=true;page.prepareFrame();});
            page.installation_.begin();
            measure("installation",[&](int) {page.frameDirty_=true;page.prepareFrame();});
            page.installation_={};
            for(const auto target:{LauncherPage::News,LauncherPage::Mods,LauncherPage::Settings,LauncherPage::Help}) {
                page.showPage(target);
                measure("page",[&](int) {page.frameDirty_=true;page.prepareFrame();});
            }
            page.showPage(LauncherPage::Home);
        }
        release(page,L"EternalMotionBenchmarkHost");
    }

    // Live captures include the native EDIT and owner-drawn buttons as Windows presents them.
    static void captureLive(const std::filesystem::path& directory) {
        std::filesystem::create_directories(directory);
        MainPage page(GetModuleHandleW(nullptr));
        if(!host(page,L"EternalLiveCaptureHost")) return;
        page.viewState_.operation=LauncherOperation::ReadyToPlay;page.viewState_.playEnabled=true;
        page.viewState_.headline=L"Pronto para jogar";page.viewState_.operationMessage=L"Os arquivos do grupo são conferidos a cada Jogar.";
        page.viewState_.primaryActionLabel=L"JOGAR";
        page.motionEnabled_=false;page.animationPhase_=.45F;page.ambientMotion_=false;
        const CLSID png=pngEncoder();
        const auto capture=[&](const std::wstring& name) {
            page.redraw();UpdateWindow(page.window_);
            RECT client{};GetClientRect(page.window_,&client);
            const auto screen=GetDC(page.window_);const auto dc=CreateCompatibleDC(screen);
            const auto bitmap=CreateCompatibleBitmap(screen,client.right,client.bottom);const auto old=SelectObject(dc,bitmap);
            check(PrintWindow(page.window_,dc,PW_CLIENTONLY)!=FALSE,"live native client capture succeeds");
            Gdiplus::Bitmap image(bitmap,nullptr);
            check(image.Save((directory/(name+L".png")).c_str(),&png)==Gdiplus::Ok,"live capture is saved with native controls");
            SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(page.window_,screen);
        };
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);
        for(const bool compact:{false,true}) {
            resize(page,compact ? 1024 : 1280,compact ? 640 : 800);
            const std::wstring suffix=compact ? L"-compact" : L"";
            page.installation_={};page.viewState_.playEnabled=true;page.showPage(LauncherPage::Home);
            capture(L"home-live"+suffix);
            page.showPage(LauncherPage::Settings);
            capture(L"settings-live"+suffix);
            page.showPage(LauncherPage::Home);
            page.viewState_.playEnabled=false;page.viewState_.operation=LauncherOperation::PreparingMinecraft;
            page.viewState_.headline=L"Minecraft e assets";page.viewState_.operationMessage=L"Baixando Minecraft, bibliotecas e assets...";
            page.viewState_.primaryActionLabel=L"PREPARANDO";page.viewState_.progressVisible=true;
            page.installation_.begin();
            page.installation_.steps[0]=InstallationStepState::Available;page.installation_.steps[1]=InstallationStepState::Active;
            auto& progress=page.installation_.progress;
            progress.stage=InstallationStage::Minecraft;progress.activity=InstallationActivity::Downloading;
            progress.message=page.viewState_.operationMessage;progress.receivedBytes=282ULL*1024*1024;progress.totalBytes=600ULL*1024*1024;
            progress.percent=47;progress.completedFiles=140;progress.totalFiles=900;progress.reusedFiles=620;
            capture(L"installation-live"+suffix);
        }
        // Keyboard: Tab moves between the native installation controls; Enter cancels.
        const auto log=control(page,InstallationLogButtonId),cancel=control(page,CancelInstallationButtonId);
        SetFocus(log);
        MSG tab{};tab.hwnd=log;tab.message=WM_KEYDOWN;tab.wParam=VK_TAB;
        check(page.handleKeyboard(tab),"live Tab is handled by native dialog navigation");
        check(GetFocus()==cancel,"live Tab moves from log to cancel");
        MSG enter{};enter.hwnd=cancel;enter.message=WM_KEYDOWN;enter.wParam=VK_RETURN;
        page.handleKeyboard(enter);
        check(page.installation_.cancelling && !IsWindowEnabled(cancel),"live Enter activates native cancellation");
        page.installation_={};
        release(page,L"EternalLiveCaptureHost");
    }
};

int main(int argc,char** argv) {
    // Match the launcher: avoid DPI virtualization in native PrintWindow captures and cadence.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    // Keep preferences, play time and test instances away from the real player data.
    std::array<wchar_t,MAX_PATH> temporary{};
    GetTempPathW(static_cast<DWORD>(temporary.size()),temporary.data());
    const auto sandbox=std::filesystem::path(temporary.data())/L"eternal-window-tests";
    std::error_code ignored;
    std::filesystem::remove_all(sandbox,ignored);
    std::filesystem::create_directories(sandbox);
    SetEnvironmentVariableW(L"APPDATA",sandbox.c_str());
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok) return 2;
    {DrawingUtils::DisplayTypeface typeface;
        if(argc==2 && std::string_view(argv[1])=="--benchmark-motion") WindowPaintingTest::benchmark();
        else if(argc==3 && std::string_view(argv[1])=="--capture-installation") WindowPaintingTest::captureLive(std::filesystem::path(argv[2]));
        else WindowPaintingTest::run();
    }
    Gdiplus::GdiplusShutdown(token);
    std::filesystem::remove_all(sandbox,ignored);
    std::cout << "Painting regression failures: " << failures << '\n';
    return failures ? 1 : 0;
}
