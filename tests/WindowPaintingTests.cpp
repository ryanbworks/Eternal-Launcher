#include "pages/MainPage.h"
#include "constants/LauncherConstants.h"
#include "utils/DrawingUtils.h"
#include "services/MinecraftLaunchCommandBuilder.h"
#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <iostream>
#include <thread>
#include <vector>

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
}
class InstallationTestLauncher final : public IMinecraftLauncherService {
public:
    int launches=0;
    MinecraftLaunchResult validateInstallation() const override {return {true,L"Pronto"};}
    MinecraftLaunchResult launch(std::wstring_view) override {++launches;return {true,L"Iniciado",123};}
    MinecraftLaunchResult launch(const MinecraftIdentity&) override {++launches;return {true,L"Iniciado",123};}
    bool isGameRunning() const override {return false;}
    std::filesystem::path gameDirectory() const override {return {};}
};
class WindowPaintingTest {
public:
    inline static std::vector<double> presentationTimes;
    static LRESULT CALLBACK benchmarkProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
        const auto result=MainPage::windowProcedure(window,message,wParam,lParam);
        if(message==WM_PAINT) presentationTimes.push_back(std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        return result;
    }
    static void benchmarkMotion() {
        MainPage page(GetModuleHandleW(nullptr));
        WNDCLASSW klass{};klass.hInstance=page.instance_;klass.lpfnWndProc=benchmarkProcedure;
        klass.lpszClassName=L"EternalMotionBenchmarkHost";klass.hbrBackground=page.backgroundBrush_;
        check(RegisterClassW(&klass)!=0,"benchmark window registration");
        page.window_=CreateWindowExW(WS_EX_COMPOSITED,klass.lpszClassName,L"Eternal — desempenho",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            CW_USEDEFAULT,CW_USEDEFAULT,1196,769,nullptr,nullptr,page.instance_,&page);
        if(!page.window_) {check(false,"benchmark host creation");return;}
        page.createActionControls();page.createNicknameInput();SetWindowTextW(page.nicknameInput_,L"Steve");
        page.settingsPage_.create(page.window_,page.instance_);page.motionEnabled_=true;
        page.viewState_.playEnabled=false;page.viewState_.progressVisible=true;page.viewState_.progressPercent=0;
        const auto measure=[](auto work) {
            std::vector<double> times;
            for(int i=0;i<48;++i) {
                const auto start=std::chrono::steady_clock::now();work(i);
                if(i>=8) times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            }
            std::sort(times.begin(),times.end());
            std::cout << " median_ms=" << times[times.size()/2] << " p95_ms=" << times[times.size()*95/100] << std::endl;
        };
        for(const bool compact:{false,true}) {
            const int width=compact ? 944 : 1180,height=compact ? 584 : 730;
            RECT desired{0,0,width,height};AdjustWindowRectEx(&desired,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,FALSE,WS_EX_COMPOSITED);
            SetWindowPos(page.window_,nullptr,0,0,desired.right-desired.left,desired.bottom-desired.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            page.layoutControls();page.installation_={};page.portalEntrance_=1;page.leftTransition_.reset();
            std::cout << "home_" << width;
            measure([&](int) {page.frameDirty_=true;page.prepareFrame();});
            std::cout << "portal_" << width;
            page.portalEnergy_=1;
            measure([&](int i) {page.portalEntrance_=static_cast<float>(i%24)/24;page.animationPhase_=static_cast<float>(i%24)/24;page.frameDirty_=true;page.prepareFrame();});
            page.portalEntrance_=1;page.portalEnergy_=0;page.installation_.begin();
            std::cout << "installation_" << width;
            measure([&](int) {page.frameDirty_=true;page.prepareFrame();});
            page.installation_={};page.showSettings();
            std::cout << "settings_" << width;
            measure([&](int i) {page.pageTravel_=static_cast<float>(i%18)/18;page.positionPageControls();page.frameDirty_=true;page.prepareFrame();});
            page.hideSettings();page.pageTravel_=1;page.positionPageControls();
            page.installation_={};page.portalHovered_=true;ShowWindow(page.window_,SW_SHOWNOACTIVATE);page.redraw();
            presentationTimes.clear();
            const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(1400);
            while(std::chrono::steady_clock::now()<end) {
                MSG message{};
                while(std::chrono::steady_clock::now()<end && PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                    TranslateMessage(&message);DispatchMessageW(&message);
                }
                Sleep(1);
            }
            std::vector<double> intervals;
            for(std::size_t i=2;i<presentationTimes.size();++i) intervals.push_back(presentationTimes[i]-presentationTimes[i-1]);
            std::sort(intervals.begin(),intervals.end());
            std::cout << "cadence_" << width << " frames=" << presentationTimes.size();
            if(!intervals.empty()) std::cout << " median_ms=" << intervals[intervals.size()/2] << " p95_ms=" << intervals[intervals.size()*95/100];
            std::cout << '\n';
            page.showSettings();presentationTimes.clear();
            const auto navigationEnd=std::chrono::steady_clock::now()+std::chrono::milliseconds(1400);
            while(std::chrono::steady_clock::now()<navigationEnd) {
                if(page.pageTravel_==1) {if(page.settingsVisible_) page.hideSettings();else page.showSettings();}
                MSG message{};
                while(std::chrono::steady_clock::now()<navigationEnd && PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                    TranslateMessage(&message);DispatchMessageW(&message);
                }
                Sleep(1);
            }
            intervals.clear();
            for(std::size_t i=2;i<presentationTimes.size();++i) intervals.push_back(presentationTimes[i]-presentationTimes[i-1]);
            std::sort(intervals.begin(),intervals.end());
            std::cout << "navigation_cadence_" << width << " frames=" << presentationTimes.size();
            if(!intervals.empty()) std::cout << " median_ms=" << intervals[intervals.size()/2] << " p95_ms=" << intervals[intervals.size()*95/100];
            std::cout << '\n';if(page.settingsVisible_) page.hideSettings();page.pageTravel_=1;
            ShowWindow(page.window_,SW_HIDE);page.redraw();
        }
        DestroyWindow(page.window_);UnregisterClassW(klass.lpszClassName,page.instance_);
    }
    static void motionRegression(MainPage& page,const LauncherViewState& initial) {
        Gdiplus::Bitmap source(32,32,PixelFormat32bppPARGB),destination(128,128,PixelFormat32bppPARGB);
        {Gdiplus::Graphics fill(&source);fill.Clear(Gdiplus::Color(255,100,50,200));}
        Gdiplus::Graphics cachedDraw(&destination);
        std::unique_ptr<Gdiplus::Bitmap> raster;
        DrawingUtils::drawCachedImage(cachedDraw,source,raster,{0,0,16,16},{0,0,32,32});
        check(raster && raster->GetWidth()==16 && raster->GetHeight()==16,"image cache is sized for the actual display");
        const auto firstRaster=raster.get();
        DrawingUtils::drawCachedImage(cachedDraw,source,raster,{0,0,16,16},{0,0,32,32});
        check(raster.get()==firstRaster,"unchanged animation frames reuse the prepared raster");
        cachedDraw.ScaleTransform(2,2);
        DrawingUtils::drawCachedImage(cachedDraw,source,raster,{0,0,16,16},{0,0,32,32});
        check(raster && raster->GetWidth()==32 && raster->GetHeight()==32,"DPI/scale changes rebuild the raster without blurry upscaling");
        check(cachedDraw.GetLastStatus()==Gdiplus::Ok,"cached image drawing succeeds");
        page.installation_={};page.viewState_=initial;
        page.viewState_.progressVisible=false;page.viewState_.progressPercent=0;
        page.hover_={};page.hovered_={};page.installationHovered_={};page.installationHover_={};
        page.portalEntrance_=1;page.displayedProgress_=0;page.originalMode_=false;page.identitySelection_=0;
        page.motionEnabled_=true;page.ambientMotion_=false;
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
        const auto tick=[&] {page.previousFrame_=std::chrono::steady_clock::now()-std::chrono::milliseconds(32);page.animate();};
        const auto childLeft=[&](HWND child) {RECT bounds{};GetWindowRect(child,&bounds);
            MapWindowPoints(nullptr,page.window_,reinterpret_cast<POINT*>(&bounds),2);return bounds.left;};
        for(const float scale:{1.0F,.8F}) {
            RECT desired{0,0,static_cast<LONG>(1180*scale),static_cast<LONG>(730*scale)};
            AdjustWindowRect(&desired,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,FALSE);
            SetWindowPos(page.window_,nullptr,0,0,desired.right-desired.left,desired.bottom-desired.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            page.layoutControls();page.redraw();
            page.showSettings();
            const HWND back=GetDlgItem(page.window_,SettingsPage::Back),edit=GetDlgItem(page.window_,SettingsPage::MinRam);
            const auto font=SendMessageW(edit,WM_GETFONT,0,0);
            const int start=childLeft(back);
            page.prepareFrame();
            check(page.settingsTransitionFrame_!=nullptr,"settings entry prepares a reusable form raster");
            const auto cachedForm=page.settingsTransitionFrame_.get();
            tick();page.prepareFrame();
            check(page.settingsTransitionFrame_.get()==cachedForm,"timer frames reuse the form without redrawing its labels");
            check(page.pageTravel_>0 && page.animationTimer_ && GetFocus()==back,"settings enter with immediate native keyboard focus");
            SetWindowTextW(edit,L"2304");SetFocus(edit);
            check(!page.settingsTransitionFrame_,"native edits/focus invalidate the form raster immediately");
            tick();
            page.prepareFrame();
            check(page.settingsTransitionFrame_!=nullptr,"updated form regenerates during the active transition");
            Gdiplus::Bitmap reference(page.frame_->GetWidth(),page.frame_->GetHeight(),PixelFormat32bppPARGB);
            {Gdiplus::Graphics g(&reference);page.render(g,false,true);}
            bool sameRestingForm=true;
            for(int y=105;y<static_cast<int>(reference.GetHeight());y+=13)
                for(int x=32;x<static_cast<int>(reference.GetWidth())-32;x+=17) {
                    Gdiplus::Color sourceColor,cachedColor;reference.GetPixel(x,y,&sourceColor);
                    page.settingsTransitionFrame_->GetPixel(x,y,&cachedColor);
                    sameRestingForm &= sourceColor.GetValue()==cachedColor.GetValue();
                }
            check(sameRestingForm,"cached form matches the current uncached labels, selected state and keyboard focus");
            const int moving=childLeft(back);
            check(moving<start && GetFocus()==edit && font==SendMessageW(edit,WM_GETFONT,0,0),
                "settings controls move with the page while preserving edit focus and font");
            std::array<wchar_t,32> draft{};GetWindowTextW(edit,draft.data(),static_cast<int>(draft.size()));
            check(std::wstring_view(draft.data())==L"2304","page animation preserves in-progress edits");
            page.showSettings();check(childLeft(back)==moving,"repeated open does not restart transition or draft");
            MSG tab{};tab.hwnd=edit;tab.message=WM_KEYDOWN;tab.wParam=VK_TAB;
            page.handleKeyboard(tab);check(GetFocus()!=edit,"Tab works while settings are moving");
            page.hideSettings();
            check(!page.settingsTransitionFrame_,"leaving settings releases the transition raster");
            check(page.pageOffset()<0 && GetFocus()==page.actions_[3],"return transition restores main actions and gear focus immediately");
            for(int i=0;i<10;++i) tick();
            check(page.pageTravel_==1 && childLeft(page.actions_[2])==page.mainControlBounds_[2].left,
                "return transition settles on exact native hit targets at both sizes");
            page.showSettings();for(int i=0;i<11;++i) tick();
            check(page.pageTravel_==1 && childLeft(back)==start-static_cast<int>(std::round(96*scale)) && !page.animationTimer_,
                "settings entrance settles and stops its timer at both sizes");
            check(!page.settingsTransitionFrame_,"settled settings releases the raster instead of retaining stale UI");
            page.hideSettings();for(int i=0;i<10;++i) tick();
        }
        RECT normal{0,0,1180,730};AdjustWindowRect(&normal,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,FALSE);
        SetWindowPos(page.window_,nullptr,0,0,normal.right-normal.left,normal.bottom-normal.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        page.layoutControls();page.redraw();
        page.originalMode_=true;page.redraw();
        check(page.animationTimer_ && page.identitySelection_==0,"identity updates immediately while its indicator starts moving");
        tick();check(page.identitySelection_>0 && page.identitySelection_<1,"identity selection interpolates without snapping");
        page.originalMode_=false;page.redraw();
        for(int i=0;i<24;++i) tick();
        check(page.identitySelection_==0 && !page.animationTimer_,"interrupted selection reverses and settles without an idle timer");
        SendMessageW(page.window_,WM_MOUSEMOVE,0,MAKELPARAM(650,330));
        check(page.portalHovered_ && page.animationTimer_,"pointer over landscape activates portal energy");
        tick();check(page.portalEnergy_>0 && page.portalEnergy_<1,"portal activation interpolates");
        SendMessageW(page.window_,WM_MOUSELEAVE,0,0);
        for(int i=0;i<28;++i) tick();
        check(page.portalEnergy_==0 && !page.animationTimer_,"portal energy stops after pointer leaves");
        SendMessageW(page.actions_[2],WM_MOUSEMOVE,0,MAKELPARAM(40,20));
        check(page.playSweep_==0 && page.animationTimer_,"play hover starts a single light sweep");
        SendMessageW(page.actions_[2],WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(40,20));tick();
        check(page.playPress_>0 && page.playPulse_>0,"native button press produces compression and ripple");
        SendMessageW(page.actions_[2],WM_CANCELMODE,0,0);
        SendMessageW(page.actions_[2],WM_MOUSELEAVE,0,0);
        for(int i=0;i<24;++i) tick();
        check(page.playPress_==0 && page.playPulse_==0 && page.playSweep_==1 && !page.animationTimer_,
            "cancelled pointer gesture releases feedback without activating play or looping");
        page.portalEntrance_=0;page.redraw();
        check(page.animationTimer_,"visible portal entrance schedules frames");
        ShowWindow(page.window_,SW_HIDE);page.redraw();
        check(!page.animationTimer_,"hidden window suspends all animation timers");
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);page.redraw();
        for(int i=0;i<24;++i) tick();
        check(page.portalEntrance_==1 && !page.animationTimer_,"portal opens once and stops rendering when settled");
        ShowWindow(page.window_,SW_SHOWMINNOACTIVE);page.redraw();
        check(!page.animationTimer_,"minimized window suspends animation");
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);page.redraw();page.prepareFrame();
        page.installation_.begin();page.viewState_.playEnabled=false;page.redraw();
        check(page.leftTransition_ && page.transitionTime_==0,"installation enters from cached home pixels");
        check(IsWindowEnabled(page.cancelInstallationButton_),"cancel remains available during the visual transition");
        tick();page.prepareFrame();
        check(page.transitionTime_>0 && page.transitionTime_<1,"installation shutter advances while controls remain usable");
        page.installation_.visible=false;page.redraw();
        check(page.leftTransition_ && page.transitionTime_==0,"rapid scene reversal restarts from the presented frame");
        for(int i=0;i<9;++i) tick();
        check(!page.leftTransition_,"completed transition releases its cached image");
        page.installation_.visible=true;page.installation_.progress.activity=InstallationActivity::Downloading;
        page.installation_.progress.percent=80;page.redraw();tick();
        check(page.installationFill_>0 && page.installationFill_<80,"real transfer fill eases toward the current percentage");
        ++page.installation_.progress.transferId;page.redraw();
        check(page.installationFill_==0,"new transfer cannot inherit the previous animated fill");
        page.installation_.steps[0]=InstallationStepState::Complete;page.redraw();
        check(page.completionPulse_[0]==1,"stage completion triggers a confirmation pulse");
        for(int i=0;i<14;++i) tick();
        check(page.completionPulse_[0]==0,"completion pulse ends without changing the stage state");
        page.portalHovered_=true;page.portalBurst_=1;page.portalEnergy_=1;page.playSweep_=.4F;page.playPulse_=1;
        page.originalMode_=true;page.installationHovered_[0]=true;page.motionEnabled_=false;page.redraw();
        check(page.identitySelection_==1 && page.installationFill_==80 && page.installationHover_[0]==1 &&
            !page.leftTransition_ && !page.animationTimer_,"reduced motion immediately preserves selection, progress and hover feedback");
        check(page.portalBurst_==0 && page.portalEnergy_==0 && page.playPulse_==0 && page.playSweep_==1,
            "reduced motion removes energy, particles, ripples and sweep");
        page.showSettings();check(page.pageTravel_==1 && !page.animationTimer_,"reduced motion opens settings without travel");
        page.hideSettings();check(page.pageTravel_==1 && !page.animationTimer_,"reduced motion returns without travel");
        page.installation_={};page.viewState_=initial;page.originalMode_=false;page.redraw();
        ShowWindow(page.window_,SW_HIDE);
    }
    static void captureMotion(const std::filesystem::path& directory) {
        std::filesystem::create_directories(directory);
        MainPage page(GetModuleHandleW(nullptr));
        WNDCLASSW klass{};klass.hInstance=page.instance_;klass.lpfnWndProc=MainPage::windowProcedure;
        klass.lpszClassName=L"EternalMotionPreviewHost";klass.hbrBackground=page.backgroundBrush_;
        check(RegisterClassW(&klass)!=0,"motion preview window registration");
        page.window_=CreateWindowExW(0,klass.lpszClassName,L"Eternal — prévia de movimento",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            CW_USEDEFAULT,CW_USEDEFAULT,1196,769,nullptr,nullptr,page.instance_,&page);
        if(!page.window_) {check(false,"motion host creation");return;}
        page.createActionControls();page.createNicknameInput();SetWindowTextW(page.nicknameInput_,L"Steve");
        page.settingsPage_.create(page.window_,page.instance_);
        page.viewState_.operation=LauncherOperation::ReadyToPlay;page.viewState_.playEnabled=true;
        page.viewState_.progressVisible=false;page.viewState_.headline=L"Pronto para jogar";
        page.viewState_.operationMessage=L"Seu mundo está preparado. Boa aventura!";page.viewState_.primaryActionLabel=L"JOGAR AGORA";
        page.motionEnabled_=true;
        page.animationPhase_=.45F;
        UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
        std::vector<BYTE> storage(size);const auto encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
        Gdiplus::GetImageEncoders(count,size,encoders);CLSID png{};
        for(UINT i=0;i<count;++i) if(std::wstring_view(encoders[i].MimeType)==L"image/png") png=encoders[i].Clsid;
        for(const bool compact:{false,true}) {
            const int width=compact ? 944 : 1180,height=compact ? 584 : 730;
            const std::wstring variant=compact ? L"compact" : L"normal";
            RECT desired{0,0,width,height};AdjustWindowRectEx(&desired,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,FALSE,0);
            SetWindowPos(page.window_,nullptr,0,0,desired.right-desired.left,desired.bottom-desired.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            page.installation_={};page.originalMode_=false;page.identitySelection_=0;
            page.settingsVisible_=false;page.settingsPage_.show(false);page.pageTravel_=1;
            page.viewState_.playEnabled=true;page.viewState_.progressVisible=false;
            page.viewState_.headline=L"Pronto para jogar";page.viewState_.operationMessage=L"Seu mundo está preparado. Boa aventura!";
            page.viewState_.primaryActionLabel=L"JOGAR AGORA";
            page.layoutControls();ShowWindow(page.window_,SW_SHOWNOACTIVATE);
            const auto capture=[&](const std::wstring& name) {
                page.positionPageControls();
                page.frameDirty_=true;check(page.prepareFrame(),"motion production frame renders");
                if(page.settingsVisible_) {
                    // Paint in client coordinates. DefWindowProc WM_PRINT/PRF_CHILDREN can add
                    // the non-client origin to child positions, duplicating buttons in this fixture.
                    RedrawWindow(page.window_,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
                    const auto screen=GetDC(page.window_);const auto dc=CreateCompatibleDC(screen);
                    const auto bitmap=CreateCompatibleBitmap(screen,width,height);const auto old=SelectObject(dc,bitmap);
                    SendMessageW(page.window_,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
                    EnumChildWindows(page.window_,[](HWND child,LPARAM context)->BOOL {
                        if(!IsWindowVisible(child)) return TRUE;
                        const auto target=reinterpret_cast<HDC>(context);
                        RECT bounds{};GetWindowRect(child,&bounds);
                        MapWindowPoints(nullptr,GetParent(child),reinterpret_cast<POINT*>(&bounds),2);
                        const int saved=SaveDC(target);
                        SetViewportOrgEx(target,bounds.left,bounds.top,nullptr);
                        IntersectClipRect(target,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top);
                        SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(target),PRF_CLIENT|PRF_ERASEBKGND);
                        RestoreDC(target,saved);return TRUE;
                    },reinterpret_cast<LPARAM>(dc));
                    Gdiplus::Bitmap image(bitmap,nullptr);
                    bool headerMatches=true;
                    for(int y=20;y<70;y+=7) for(int x=32;x<500;x+=17) {
                        Gdiplus::Color sourceColor,capturedColor;page.frame_->GetPixel(x,y,&sourceColor);image.GetPixel(x,y,&capturedColor);
                        headerMatches &= sourceColor.GetR()==capturedColor.GetR() && sourceColor.GetG()==capturedColor.GetG() && sourceColor.GetB()==capturedColor.GetB();
                    }
                    check(headerMatches,"native preview contains the current correctly scaled fixed header");
                    check(image.Save((directory/(name+L"-"+variant+L".png")).c_str(),&png)==Gdiplus::Ok,"native settings frame saved");
                    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(page.window_,screen);return;
                }
                auto image=std::unique_ptr<Gdiplus::Bitmap>(page.frame_->Clone(Gdiplus::Rect(0,0,width,height),PixelFormat32bppPARGB));
                // Frame rendering excludes the native EDIT; use the same safe synthetic nickname as other previews.
                if(!page.originalMode_) {
                    Gdiplus::Graphics g(image.get());g.TranslateTransform(page.offsetX_,page.offsetY_);g.ScaleTransform(page.viewScale_,page.viewScale_);
                    g.TranslateTransform(page.pageOffset(),0);
                    DrawingUtils::drawText(g,L"Steve",{846,289,260,44},15,Gdiplus::FontStyleRegular,Gdiplus::Color(255,245,247,250),
                        Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
                }
                check(image->Save((directory/(name+L"-"+variant+L".png")).c_str(),&png)==Gdiplus::Ok,"motion frame saved");
            };
            page.leftTransition_.reset();page.previousScene_=0;
            for(int i=0;i<=38;++i) {
                page.portalEntrance_=std::min(1.0F,static_cast<float>(i)*20/750);
                page.portalBurst_=std::max(0.0F,1-static_cast<float>(i)*20/1400);
                page.animationPhase_=std::fmod(static_cast<float>(i)*20/1600,1.0F);
                page.particlePhase_=static_cast<float>(i)*20/8000;
                capture(L"portal-"+(i<10 ? std::wstring(L"0") : std::wstring())+std::to_wstring(i));
            }
            page.portalBurst_=0;page.portalEnergy_=1;
            for(int i=0;i<=40;++i) {
                page.animationPhase_=std::fmod(.48F+static_cast<float>(i)*20/1600,1.0F);
                page.hover_[2]=i<24 ? 1-std::exp(-static_cast<float>(i)*20/35) : std::exp(-static_cast<float>(i-24)*20/35);
                page.hover_[3]=i<24 ? 1-std::exp(-static_cast<float>(i)*20/35) : std::exp(-static_cast<float>(i-24)*20/35);
                page.playSweep_=std::min(1.0F,static_cast<float>(i)*20/480);
                page.playPulse_=i<24 ? 0 : std::max(0.0F,1-static_cast<float>(i-24)*20/380);
                page.playPress_=i>=24 && i<29 ? .9F : 0;
                page.playLight_=.15F+.7F*static_cast<float>(i)/40;
                page.particlePhase_=.1F+static_cast<float>(i)*20/8000;
                SendMessageW(page.actions_[2],BM_SETSTATE,i>=24 && i<29,0);
                capture(L"interaction-"+std::to_wstring(i));
            }
            SendMessageW(page.actions_[2],BM_SETSTATE,FALSE,0);page.playPress_=0;page.playPulse_=0;page.playSweep_=1;
            page.portalEnergy_=0;page.hover_={};
            page.settings_=LauncherSettings{};page.settings_.gameDirectory=L"C:\\Eternal\\instances\\Eternal";
            page.showSettings();
            for(int i=0;i<=17;++i) {page.pageTravel_=static_cast<float>(i)/17;capture(L"settings-in-"+std::to_wstring(i));}
            page.hideSettings();
            for(int i=0;i<=14;++i) {page.pageTravel_=static_cast<float>(i)/14;capture(L"settings-out-"+std::to_wstring(i));}
            page.originalMode_=true;
            for(int i=0;i<=18;++i) {
                page.identitySelection_=i==18 ? 1 : 1-std::exp(-static_cast<float>(i)*20/55);
                capture(L"identity-"+std::to_wstring(i));
            }
            page.originalMode_=false;page.identitySelection_=0;page.frameDirty_=true;page.prepareFrame();
            page.installation_.begin();page.installation_.progress.message=L"Verificando os arquivos da instalação...";
            page.viewState_.playEnabled=false;page.viewState_.progressVisible=true;
            page.viewState_.headline=L"Verificações e Java 17";page.viewState_.operationMessage=page.installation_.progress.message;
            page.viewState_.primaryActionLabel=L"PREPARANDO...";
            page.redraw();
            for(int i=0;i<=12;++i) {page.transitionTime_=static_cast<float>(i)/12;capture(L"installation-"+std::to_wstring(i));}
            page.leftTransition_.reset();page.transitionTime_=1;
            page.installation_.steps[0]=InstallationStepState::Complete;page.installation_.steps[1]=InstallationStepState::Active;
            page.installation_.progress.stage=InstallationStage::Minecraft;page.installation_.progress.activity=InstallationActivity::Downloading;
            page.installation_.progress.message=L"Baixando Minecraft, bibliotecas e assets...";
            page.installation_.progress.percent=47;page.installationFill_=47;
            page.installation_.progress.receivedBytes=47ULL*1024*1024;page.installation_.progress.totalBytes=100ULL*1024*1024;
            page.viewState_.headline=L"Minecraft e assets";page.viewState_.operationMessage=page.installation_.progress.message;
            for(int i=0;i<=21;++i) {
                page.completionPulse_[0]=std::max(0.0F,1-static_cast<float>(i)/21);
                page.animationPhase_=std::fmod(static_cast<float>(i)*20/1600,1.0F);
                capture(L"completion-"+std::to_wstring(i));
            }
            page.completionPulse_={};
        }
        DestroyWindow(page.window_);UnregisterClassW(klass.lpszClassName,page.instance_);
    }
    static void captureInstallation(const std::filesystem::path& directory) {
        std::filesystem::create_directories(directory);
        MainPage page(GetModuleHandleW(nullptr));
        WNDCLASSW klass{};klass.hInstance=page.instance_;klass.lpfnWndProc=MainPage::windowProcedure;
        klass.lpszClassName=L"EternalInstallationLiveTestHost";klass.hbrBackground=page.backgroundBrush_;
        check(RegisterClassW(&klass)!=0,"live test window registration");
        page.window_=CreateWindowExW(0,klass.lpszClassName,L"Eternal — teste de instalação",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            CW_USEDEFAULT,CW_USEDEFAULT,1196,769,nullptr,nullptr,page.instance_,&page);
        if(!page.window_) {check(false,"live installation host creation");return;}
        page.createActionControls();page.createNicknameInput();
        SetWindowTextW(page.nicknameInput_,L"Steve");
        check(page.settingsPage_.create(page.window_,page.instance_),"live native settings controls");
        page.viewState_.playEnabled=false;page.viewState_.operation=LauncherOperation::PreparingMinecraft;
        page.viewState_.headline=L"Minecraft e assets";page.viewState_.operationMessage=L"Baixando Minecraft, bibliotecas e assets...";
        page.viewState_.primaryActionLabel=L"PREPARANDO...";page.viewState_.progressVisible=true;
        page.installation_.begin();
        page.installation_.steps[0]=InstallationStepState::Available;page.installation_.steps[1]=InstallationStepState::Active;
        auto& progress=page.installation_.progress;
        progress.stage=InstallationStage::Minecraft;progress.activity=InstallationActivity::Downloading;
        progress.message=page.viewState_.operationMessage;progress.receivedBytes=282ULL*1024*1024;progress.totalBytes=600ULL*1024*1024;
        progress.percent=47;progress.completedFiles=140;progress.totalFiles=900;progress.reusedFiles=620;
        page.motionEnabled_=false;page.animationPhase_=.45F;
        page.layoutControls();page.redraw();
        ShowWindow(page.window_,SW_SHOWNOACTIVATE);UpdateWindow(page.window_);
        SetFocus(page.installationLogButton_);
        MSG tab{};tab.hwnd=page.installationLogButton_;tab.message=WM_KEYDOWN;tab.wParam=VK_TAB;
        check(page.handleKeyboard(tab),"live Tab is handled by native dialog navigation");
        check(GetFocus()==page.cancelInstallationButton_,"live Tab moves from log to cancel");
        SetFocus(page.cancelInstallationButton_);
        MSG enter{};enter.hwnd=page.cancelInstallationButton_;enter.message=WM_KEYDOWN;enter.wParam=VK_RETURN;
        page.handleKeyboard(enter);
        check(page.installation_.cancelling && !IsWindowEnabled(page.cancelInstallationButton_),"live Enter activates native cancellation");
        // Restore download state for capture; this host never starts a worker or accesses the network.
        page.installation_.cancelling=false;page.viewState_.headline=L"Minecraft e assets";
        page.viewState_.operationMessage=progress.message;page.viewState_.primaryActionLabel=L"PREPARANDO...";
        UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
        std::vector<BYTE> storage(size);const auto encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
        Gdiplus::GetImageEncoders(count,size,encoders);CLSID png{};
        for(UINT i=0;i<count;++i) if(std::wstring_view(encoders[i].MimeType)==L"image/png") png=encoders[i].Clsid;
        for(const bool compact:{false,true}) {
            const int width=compact ? 944 : 1180,height=compact ? 584 : 730;
            RECT desired{0,0,width,height};AdjustWindowRectEx(&desired,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,FALSE,0);
            SetWindowPos(page.window_,nullptr,0,0,desired.right-desired.left,desired.bottom-desired.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            page.layoutControls();page.redraw();UpdateWindow(page.window_);
            const auto screen=GetDC(page.window_);const auto dc=CreateCompatibleDC(screen);
            const auto bitmap=CreateCompatibleBitmap(screen,width,height);const auto old=SelectObject(dc,bitmap);
            check(PrintWindow(page.window_,dc,PW_CLIENTONLY)!=FALSE,"live native client capture succeeds");
            Gdiplus::Bitmap capture(bitmap,nullptr);
            check(capture.Save((directory/(compact ? L"installation-live-compact.png" : L"installation-live.png")).c_str(),&png)==Gdiplus::Ok,
                "live capture is saved with native controls");
            SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(page.window_,screen);
        }
        DestroyWindow(page.window_);UnregisterClassW(klass.lpszClassName,page.instance_);
    }
    static void run() {
        MainPage page(GetModuleHandleW(nullptr));
        WNDCLASSW klass{};
        klass.hInstance=page.instance_;
        klass.lpszClassName=L"EternalPaintingRegressionHost";
        klass.lpfnWndProc=MainPage::windowProcedure;
        klass.hbrBackground=page.backgroundBrush_;
        check(RegisterClassW(&klass)!=0,"test-owned window registration");
        // Keep this host hidden: no network, downloads, preference writes or desktop control.
        page.window_=CreateWindowExW(0,klass.lpszClassName,L"Painting regression",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            0,0,1196,769,nullptr,nullptr,page.instance_,&page);
        check(page.window_!=nullptr,"hidden production window procedure");
        if (!page.window_) return;
        page.viewState_.playEnabled=true;
        page.viewState_.operation=LauncherOperation::ReadyToPlay;
        page.viewState_.primaryActionLabel=L"JOGAR AGORA";
        page.createActionControls();page.createNicknameInput();
        check(page.settingsPage_.create(page.window_,page.instance_),"settings native children");
        page.layoutControls();page.syncControls();
        page.motionEnabled_=false;
        const auto initialState=page.viewState_;
        page.updatingLauncher_=true;
        for(const auto phase:{LauncherUpdatePhase::Checking,LauncherUpdatePhase::Downloading,
            LauncherUpdatePhase::Applying,LauncherUpdatePhase::Restarting}) {
            page.applyLauncherUpdateProgress({phase,phase==LauncherUpdatePhase::Checking ? 0 : 47});
            check(!page.viewState_.playEnabled && page.viewState_.progressVisible,"launcher update must own progress and block play");
            for(const auto control:page.actions_) check(!IsWindowEnabled(control),"all actions must be disabled during self update");
        }
        page.updatingLauncher_=false;page.viewState_=initialState;
        page.updateWarning_=L"A atualização falhou. A versão anterior foi restaurada.";
        page.syncControls();
        check((GetWindowLongPtrW(page.updateDownloadButton_,GWL_STYLE)&WS_VISIBLE)!=0,"recovery must expose manual download");
        check(IsWindowEnabled(page.actions_[2]),"update failure must let the installed launcher continue");
        SendMessageW(page.updateDownloadButton_,WM_MOUSEMOVE,0,MAKELPARAM(4,4));
        SendMessageW(page.updateDownloadButton_,WM_MOUSELEAVE,0,0);
        page.updateWarning_.clear();page.syncControls();
        check((GetWindowLongPtrW(page.updateDownloadButton_,GWL_STYLE)&WS_VISIBLE)==0,"manual download must hide without a warning");
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
        const auto buttonCheck=[&](HWND button,const char* eraseDescription) {
            FillRect(dc,&bounds,dark);
            const auto erased=SendMessageW(button,WM_ERASEBKGND,reinterpret_cast<WPARAM>(dc),0);
            const auto color=GetPixel(dc,2,2);
            std::cout << eraseDescription << ": handled=" << erased << ", pixel=" << color << '\n';
            check(erased!=0 && color==RGB(13,14,20),eraseDescription);
            const auto brush=reinterpret_cast<HBRUSH>(SendMessageW(page.window_,WM_CTLCOLORBTN,
                reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(button)));
            check(brushColor(brush)==RGB(13,14,20),"owner-drawn button fallback brush must be dark");
        };
        buttonCheck(page.actions_[3],"main button erasure must not expose a light background");
        drawButton(page.actions_[3]);
        const auto mainIdle=GetPixel(dc,4,20);
        validateChildren(page.window_);
        SendMessageW(page.actions_[3],WM_MOUSEMOVE,0,MAKELPARAM(4,4));
        check(!GetUpdateRect(page.nicknameInput_,nullptr,FALSE),"main hover must leave native text field paint untouched");
        drawButton(page.actions_[3]);
        check(GetPixel(dc,4,20)!=mainIdle,"main child must see updated hover even before parent WM_PAINT");
        SendMessageW(page.actions_[3],WM_MOUSELEAVE,0,0);
        page.showSettings();
        page.settingsPage_.open(LauncherSettings{},{});
        page.redraw();
        const auto slider=GetDlgItem(page.window_,SettingsPage::RamSlider);
        LauncherSettings memory;
        check(SendMessageW(slider,TBM_GETPOS,0,0)==8,"default 4 GB must map to slider step 8");
        for (const int step:{2,3,4,9,13}) {
            SendMessageW(slider,TBM_SETPOS,FALSE,step);
            SendMessageW(page.window_,WM_HSCROLL,MAKEWPARAM(TB_THUMBTRACK,step),reinterpret_cast<LPARAM>(slider));
            check(page.settingsPage_.collect(memory) && memory.maximumMemoryMegabytes==step*512,
                "slider notifications must collect 1, 1.5, 2, 4.5 and 6.5 GB without rounding");
            const auto command=MinecraftLaunchCommandBuilder{}.build(MinecraftIdentity{L"Steve"},L"C:/Java/bin/java.exe",
                L"C:/Eternal Test/minecraft",memory);
            check(command.find(L"-Xmx"+std::to_wstring(step*512)+L"M")!=command.npos,
                "half-step slider value must reach the JVM launch argument");
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
        check(!IsWindowEnabled(slider) && !IsWindowEnabled(GetDlgItem(page.window_,SettingsPage::Ram4)),"busy operation must disable slider and presets");
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
            SendMessageW(slider,TBM_GETRANGEMAX,0,0)==32,
            "restore must synchronize default memory range and enabled minimize preference");
        buttonCheck(slider,"slider erasure must preserve dark background");
        FillRect(dc,&bounds,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SendMessageW(slider,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
        check(GetPixel(dc,1,1)==RGB(13,14,20),"native slider painting must cover its background without a white flash");
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
        // Owner drawing must fill its whole destination, including rounded corners.
        const auto white=GetStockObject(WHITE_BRUSH);
        for(int frame=0;frame<24;++frame) {
            SendMessageW(save,frame%2 ? WM_MOUSELEAVE : WM_MOUSEMOVE,0,MAKELPARAM(4,4));
            FillRect(dc,&bounds,static_cast<HBRUSH>(white));
            drawButton(save);
            check(GetPixel(dc,1,1)!=RGB(255,255,255),"owner draw must cover every frame after hover changes");
        }
        page.settingsVisible_=false;page.settingsPage_.show(false);
        page.viewState_=initialState;
        auto fake=std::make_unique<InstallationTestLauncher>();const auto fakePointer=fake.get();
        page.minecraftLauncherService_=std::move(fake);
        page.settings_.minimizeOnLaunch=false;
        page.installation_.begin();page.installationMailbox_.begin(page.installation_.attempt);
        page.viewState_.playEnabled=false;page.viewState_.progressVisible=true;
        page.layoutControls();page.redraw();
        check((GetWindowLongPtrW(page.cancelInstallationButton_,GWL_STYLE)&WS_VISIBLE)!=0,"installation exposes native cancel control");
        check((GetWindowLongPtrW(page.installationLogButton_,GWL_STYLE)&WS_VISIBLE)!=0,"installation exposes native log control");
        check(!IsWindowEnabled(page.actions_[2]) && IsWindowEnabled(page.cancelInstallationButton_),"preparation blocks play and allows cancellation");
        buttonCheck(page.cancelInstallationButton_,"installation cancel must preserve dark background");
        drawButton(page.cancelInstallationButton_);
        check(GetPixel(dc,2,2)!=RGB(255,255,255),"installation button receives production owner-drawn pixels");
        const auto attempt=page.installation_.attempt;
        SendMessageW(page.window_,WM_COMMAND,MAKEWPARAM(LauncherConstants::PlayButtonId,BN_CLICKED),reinterpret_cast<LPARAM>(page.actions_[2]));
        check(page.installation_.attempt==attempt,"repeated play cannot start another active attempt");
        SendMessageW(page.cancelInstallationButton_,BM_CLICK,0,0);
        SendMessageW(page.cancelInstallationButton_,BM_CLICK,0,0);
        check(page.installation_.cancelling && !IsWindowEnabled(page.cancelInstallationButton_),"native cancel is idempotent and disables repeated clicks");
        page.installationMailbox_.finish(attempt,{InstallationOutcome::Success});page.consumeInstallationProgress();
        check(fakePointer->launches==0 && page.installation_.result->outcome==InstallationOutcome::Cancelled,"cancel wins queued successful preparation");
        check(page.viewState_.playEnabled && page.viewState_.primaryActionLabel==L"CONTINUAR INSTALAÇÃO","cancelled installation can continue");
        page.installation_.begin();page.installationMailbox_.begin(page.installation_.attempt);
        page.viewState_.playEnabled=false;
        page.installationMailbox_.finish(attempt,{InstallationOutcome::Success});page.consumeInstallationProgress();
        check(fakePointer->launches==0 && page.installation_.running,"old attempt result cannot launch a new attempt");
        page.installationMailbox_.finish(page.installation_.attempt,{InstallationOutcome::Success,InstallationFailure::None,L"Pronto",{},3});
        page.consumeInstallationProgress();
        check(fakePointer->launches==1 && !page.installation_.visible && page.viewState_.operation==LauncherOperation::MinecraftRunning,
            "successful preparation launches exactly once and restores landscape");

        motionRegression(page,initialState);

        page.installation_.begin();page.installationMailbox_.begin(page.installation_.attempt);
        page.viewState_.playEnabled=false;page.viewState_.progressVisible=true;
        const auto closingAttempt=page.installation_.attempt;
        page.installationThread_=std::jthread([&](std::stop_token stop) {
            while(!stop.stop_requested()) Sleep(5);
            Sleep(80); // Assert that WM_CLOSE keeps pumping during worker cleanup.
            page.installationMailbox_.finish(closingAttempt,{InstallationOutcome::Cancelled});
            PostMessageW(page.window_,LauncherConstants::InstallationProgressMessage,0,0);
        });
        SetWindowTextW(page.nicknameInput_,L""); // The test must never save a real nickname.
        const auto closeStart=GetTickCount64();SendMessageW(page.window_,WM_CLOSE,0,0);
        check(GetTickCount64()-closeStart<60 && IsWindow(page.window_),"closing during preparation defers destruction without blocking");
        const auto deadline=GetTickCount64()+2000;
        while(IsWindow(page.window_) && GetTickCount64()<deadline) {
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&message);DispatchMessageW(&message);}
            Sleep(5);
        }
        check(!IsWindow(page.window_) && fakePointer->launches==1,"close waits for cleanup and never launches pending installation");
        SelectObject(dc,previous);DeleteObject(bitmap);DeleteObject(dark);DeleteDC(dc);
        if(IsWindow(page.window_)) DestroyWindow(page.window_);
        UnregisterClassW(klass.lpszClassName,page.instance_);
    }
};
int main(int argc,char** argv) {
    // Match the launcher: avoid DPI virtualization in native PrintWindow captures and cadence.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok) return 2;
    {DrawingUtils::DisplayTypeface typeface;
        if(argc==2 && std::string_view(argv[1])=="--benchmark-motion") WindowPaintingTest::benchmarkMotion();
        else if(argc==3 && std::string_view(argv[1])=="--capture-motion") WindowPaintingTest::captureMotion(std::filesystem::path(argv[2]));
        else if(argc==3 && std::string_view(argv[1])=="--capture-installation") WindowPaintingTest::captureInstallation(std::filesystem::path(argv[2]));
        else WindowPaintingTest::run();
    }
    Gdiplus::GdiplusShutdown(token);
    std::cout << "Painting regression failures: " << failures << '\n';
    return failures ? 1 : 0;
}
