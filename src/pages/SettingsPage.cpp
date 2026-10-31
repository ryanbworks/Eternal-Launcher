#include "pages/SettingsPage.h"
#include "components/Buttons.h"
#include "config/LauncherConfig.h"
#include "constants/LauncherConstants.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"
#include "utils/Icons.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <locale>
#include <optional>

namespace {
    const wchar_t* Labels[] = {L"Geral",L"Avançado",L"Sobre",
        L"RAM inicial (MiB)",L"RAM máxima (MiB)",L"Largura (pixels)",L"Altura (pixels)",
        L"Abrir em tela cheia",L"Esconder na bandeja ao jogar",L"Java automático",L"Java personalizado",
        L"Executável Java 17",L"Escolher Java",L"Pasta da instância",L"Escolher pasta",
        L"Abrir jogo",L"Abrir mods",L"Abrir logs",L"Restaurar padrões",L"Descartar",L"Salvar",
        L"RAM máxima em passos de 0,5 GB",L"4 GB",L"6 GB",L"8 GB",L"Ver changelog",L"Repositório no GitHub",L"Pasta de dados"};
    std::optional<int> memoryNumber(std::wstring_view text) {
        if (text.empty() || text.size()>5) return {};
        int value=0;
        for (const auto c:text) {
            if (c<L'0' || c>L'9') return {};
            value=value*10+(c-L'0');
        }
        return value;
    }
    void card(Gdiplus::Graphics& g,const Gdiplus::RectF& b,std::wstring_view title,std::wstring_view subtitle) {
        using namespace DrawingUtils;
        fillRoundedRectangle(g,b,18,Theme::Surface);
        drawRoundedRectangle(g,{b.X+.5F,b.Y+.5F,b.Width-1,b.Height-1},18,Theme::Divider);
        drawText(g,title,{b.X+24,b.Y+22,b.Width-48,24},16,Gdiplus::FontStyleBold,Theme::TextPrimary);
        drawText(g,subtitle,{b.X+24,b.Y+48,b.Width-48,36},12.5F,Gdiplus::FontStyleRegular,Theme::TextMuted);
    }
    const Gdiplus::RectF MemoryCard{100,228,568,252};
    const Gdiplus::RectF WindowCard{688,228,568,288};
    const Gdiplus::RectF ExactCard{100,228,568,184};
    const Gdiplus::RectF FolderCard{100,428,568,272};
    const Gdiplus::RectF JavaCard{688,228,568,472};
    const Gdiplus::RectF AboutCard{100,228,568,472};
    const Gdiplus::RectF CreditsCard{688,228,568,472};
}
SettingsPage::~SettingsPage()
{
    for (const auto window:controls_) if (IsWindow(window)) DestroyWindow(window);
    if (font_) DeleteObject(font_);
}
bool SettingsPage::isEdit(int id)
{
    return id==MinRam || id==MaxRam || id==Width || id==Height || id==JavaPath || id==GamePath;
}
SettingsPage::Tab SettingsPage::tabOf(int id)
{
    switch(id) {
    case MinRam:case MaxRam:case JavaAuto:case JavaCustom:case JavaPath:case JavaBrowse:
    case GamePath:case GameBrowse:case OpenGame:case OpenMods:case OpenLogs:return Tab::Advanced;
    case OpenChangelog:case OpenRepository:case OpenDataFolder:return Tab::About;
    default:return Tab::General;
    }
}
bool SettingsPage::isAlwaysVisible(int id)
{
    return id==TabGeneral || id==TabAdvanced || id==TabAbout || id==Restore || id==Cancel || id==Save;
}
int SettingsPage::recommendedMemory()
{
    const auto total=LauncherSettingsStore::physicalMemoryMegabytes();
    if (total>=24576) return 8192;
    if (total>=12288) return 6144;
    return 4096;
}
Gdiplus::RectF SettingsPage::bounds(int id)
{
    switch(id) {
    case TabGeneral:return {100,160,92,44};
    case TabAdvanced:return {196,160,112,44};
    case TabAbout:return {312,160,84,44};
    case RamSlider:return {124,338,520,32};
    case Ram4:return {124,396,165,56};
    case Ram6:return {301,396,165,56};
    case Ram8:return {478,396,166,56};
    case Width:return {712,338,248,42};
    case Height:return {984,338,248,42};
    case Fullscreen:return {712,396,520,48};
    case Minimize:return {712,446,520,48};
    case MinRam:return {124,346,248,42};
    case MaxRam:return {396,346,248,42};
    case GamePath:return {124,512,430,42};
    case GameBrowse:return {566,512,78,42};
    case OpenGame:return {124,628,165,44};
    case OpenMods:return {301,628,165,44};
    case OpenLogs:return {478,628,166,44};
    case JavaAuto:return {712,316,254,48};
    case JavaCustom:return {978,316,254,48};
    case JavaPath:return {712,408,432,42};
    case JavaBrowse:return {1154,408,78,42};
    case OpenChangelog:return {124,452,190,44};
    case OpenRepository:return {326,452,220,44};
    case OpenDataFolder:return {124,508,190,44};
    case Restore:return {872,726,164,46};
    case Cancel:return {1044,726,104,46};
    case Save:return {1156,726,100,46};
    default:return {};
    }
}
bool SettingsPage::create(HWND parent,HINSTANCE instance)
{
    parent_=parent; instance_=instance;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&common)) return false;
    for (int id=TabGeneral;id<ControlEnd;++id) {
        const bool edit=isEdit(id);
        const bool slider=id==RamSlider;
        DWORD style=WS_CHILD | WS_TABSTOP | (edit ? ES_AUTOHSCROLL : slider ? TBS_HORZ|TBS_NOTICKS|TBS_FIXEDLENGTH : BS_OWNERDRAW);
        if (edit && id!=JavaPath && id!=GamePath) style |= ES_NUMBER;
        controls_[id-TabGeneral]=CreateWindowExW(0,edit ? L"EDIT" : slider ? TRACKBAR_CLASSW : L"BUTTON",edit ? L"" : Labels[id-TabGeneral],
            style,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
        if (!control(id)) return false;
        SetWindowSubclass(control(id),controlProcedure,static_cast<UINT_PTR>(id),reinterpret_cast<DWORD_PTR>(this));
        if (edit) {
            SendMessage(control(id),EM_SETLIMITTEXT,id==JavaPath || id==GamePath ? 4096 : 5,0);
            SetWindowTheme(control(id),L"DarkMode_Explorer",nullptr);
        }
    }
    SetWindowTheme(control(RamSlider),L"",L"");
    SendMessage(control(RamSlider),TBM_SETLINESIZE,0,1);
    SendMessage(control(RamSlider),TBM_SETPAGESIZE,0,2);
    const int tabOrder[]={TabGeneral,TabAdvanced,TabAbout,RamSlider,Ram4,Ram6,Ram8,Width,Height,Fullscreen,Minimize,
        MinRam,MaxRam,GamePath,GameBrowse,OpenGame,OpenMods,OpenLogs,JavaAuto,JavaCustom,JavaPath,JavaBrowse,
        OpenChangelog,OpenRepository,OpenDataFolder,Restore,Cancel,Save};
    HWND previous=HWND_BOTTOM;
    for (const auto id:tabOrder) {
        SetWindowPos(control(id),previous,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        previous=control(id);
    }
    SendMessage(control(JavaPath),EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Escolha java.exe ou javaw.exe do Java 17"));
    SendMessage(control(GamePath),EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Automática: %APPDATA%\\.eternal-launcher\\minecraft"));
    return true;
}
void SettingsPage::populate(const LauncherSettings& s)
{
    updatingMemory_=true;
    draft_=s; automaticJava_=s.javaExecutable.empty();
    sliderMaximumSteps_=32;
    SetWindowTextW(control(MinRam),std::to_wstring(s.minimumMemoryMegabytes).c_str());
    SetWindowTextW(control(MaxRam),std::to_wstring(s.maximumMemoryMegabytes).c_str());
    SetWindowTextW(control(Width),std::to_wstring(s.windowWidth).c_str());
    SetWindowTextW(control(Height),std::to_wstring(s.windowHeight).c_str());
    SetWindowTextW(control(JavaPath),s.javaExecutable.c_str());
    SetWindowTextW(control(GamePath),s.gameDirectory.c_str());
    syncMemoryControls(s.maximumMemoryMegabytes);
    updatingMemory_=false;
    settle();
    setLocked(locked_);
}
std::wstring SettingsPage::memoryLabel(int megabytes)
{
    if (megabytes%1024==0) return std::to_wstring(megabytes/1024)+L" GB";
    std::wostringstream value;
    value.imbue(std::locale::classic());
    value<<std::fixed<<std::setprecision(megabytes%512==0 ? 1 : 2)<<megabytes/1024.0;
    auto label=value.str();
    std::replace(label.begin(),label.end(),L'.',L',');
    return label+L" GB";
}
void SettingsPage::syncMemoryControls(int megabytes)
{
    if (megabytes<512 || megabytes>65536) return;
    draft_.maximumMemoryMegabytes=megabytes;
    sliderMaximumSteps_=std::max(sliderMaximumSteps_,(megabytes+511)/512);
    if (control(RamSlider)) {
        SendMessage(control(RamSlider),TBM_SETRANGEMIN,FALSE,1);
        SendMessage(control(RamSlider),TBM_SETRANGEMAX,FALSE,sliderMaximumSteps_);
        SendMessage(control(RamSlider),TBM_SETPOS,FALSE,(megabytes+256)/512);
        const auto name=L"RAM máxima: "+memoryLabel(megabytes)+L". Passos de 0,5 GB.";
        SetWindowTextW(control(RamSlider),name.c_str());
    }
}
void SettingsPage::selectMemory(int megabytes)
{
    if (locked_) return;
    updatingMemory_=true;
    syncMemoryControls(megabytes);
    SetWindowTextW(control(MaxRam),std::to_wstring(megabytes).c_str());
    const auto initial=memoryNumber(text(MinRam));
    if (initial && *initial>megabytes) {
        draft_.minimumMemoryMegabytes=megabytes;
        SetWindowTextW(control(MinRam),std::to_wstring(megabytes).c_str());
    }
    updatingMemory_=false;
    setMessage(L"RAM máxima selecionada: "+memoryLabel(megabytes)+L". Salve para aplicar.");
    setLocked(locked_);
    redraw();
}
void SettingsPage::handleMemoryScroll(HWND slider)
{
    if (slider!=control(RamSlider) || !slider || locked_ || updatingMemory_) return;
    selectMemory(static_cast<int>(SendMessage(slider,TBM_GETPOS,0,0))*512);
}
void SettingsPage::open(const LauncherSettings& settings,std::wstring_view warning)
{
    saved_=settings; populate(settings);
    setMessage(std::wstring(warning),!warning.empty());
}
void SettingsPage::applyVisibility()
{
    for (int id=TabGeneral;id<ControlEnd;++id) {
        if (!control(id)) continue;
        const bool shown=visible_ && (isAlwaysVisible(id) || tabOf(id)==tab_);
        ShowWindow(control(id),shown ? SW_SHOWNA : SW_HIDE);
    }
}
void SettingsPage::show(bool visible)
{
    visible_=visible;
    applyVisibility();
}
void SettingsPage::setTab(Tab tab)
{
    if (tab_==tab) return;
    tab_=tab;
    const auto focus=GetFocus();
    bool focusHidden=false;
    for (int id=TabGeneral;id<ControlEnd;++id) if (focus==control(id) && !isAlwaysVisible(id) && tabOf(id)!=tab_) focusHidden=true;
    applyVisibility();
    if (focusHidden && visible_) SetFocus(control(tab_==Tab::General ? TabGeneral : tab_==Tab::Advanced ? TabAdvanced : TabAbout));
    redraw();
}
void SettingsPage::layout(float scale,float ox,float oy)
{
    controlScale_=scale;
    const auto font=CreateFontW(-static_cast<int>(std::round(14*scale)),0,0,0,FW_NORMAL,
        FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,DEFAULT_PITCH | FF_DONTCARE,Theme::FontFamily);
    for(int id=TabGeneral;id<ControlEnd;++id) {
        if(!control(id)) continue;
        auto b=bounds(id);
        if(isEdit(id)) {b.X+=12; b.Y+=11; b.Width-=24; b.Height-=18;}
        const int x=static_cast<int>(std::floor(ox+b.X*scale)),y=static_cast<int>(std::floor(oy+b.Y*scale));
        const int r=static_cast<int>(std::ceil(ox+b.GetRight()*scale)),bottom=static_cast<int>(std::ceil(oy+b.GetBottom()*scale));
        SetWindowPos(control(id),nullptr,x,y,r-x,bottom-y,SWP_NOZORDER|SWP_NOACTIVATE);
        if(font) SendMessage(control(id),WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    }
    if(font) {if(font_) DeleteObject(font_);font_=font;}
    SendMessage(control(RamSlider),TBM_SETTHUMBLENGTH,static_cast<WPARAM>(std::round(20*scale)),0);
}
void SettingsPage::setLocked(bool locked)
{
    locked_=locked;
    for(int id=TabGeneral;id<ControlEnd;++id) if(control(id)) {
        const bool alwaysAvailable=id==TabGeneral || id==TabAdvanced || id==TabAbout || id==Cancel ||
            id==OpenGame || id==OpenMods || id==OpenLogs || id==OpenChangelog || id==OpenRepository || id==OpenDataFolder;
        const bool editable=alwaysAvailable || !locked;
        const bool javaEditable=!(id==JavaPath || id==JavaBrowse) || !automaticJava_;
        const bool windowEditable=!(id==Width || id==Height) || !draft_.fullscreen;
        const bool enabled=editable && javaEditable && windowEditable;
        if ((IsWindowEnabled(control(id))!=FALSE)!=enabled) EnableWindow(control(id),enabled);
    }
    const auto label=[&](int id,bool selected) {
        const std::wstring value=std::wstring(Labels[id-TabGeneral])+(selected ? L" (selecionado)" : L"");
        std::array<wchar_t,128> existing{}; GetWindowTextW(control(id),existing.data(),static_cast<int>(existing.size()));
        if(value!=existing.data()) SetWindowTextW(control(id),value.c_str());
    };
    if(control(JavaAuto)) {
        label(JavaAuto,automaticJava_);label(JavaCustom,!automaticJava_);
        label(Fullscreen,draft_.fullscreen);label(Minimize,draft_.minimizeOnLaunch);
        label(Ram4,draft_.maximumMemoryMegabytes==4096);
        label(Ram6,draft_.maximumMemoryMegabytes==6144);
        label(Ram8,draft_.maximumMemoryMegabytes==8192);
        label(TabGeneral,tab_==Tab::General);label(TabAdvanced,tab_==Tab::Advanced);label(TabAbout,tab_==Tab::About);
    }
}
std::wstring SettingsPage::text(int id) const
{
    if(!control(id)) {
        switch(id) {
        case MinRam:return std::to_wstring(draft_.minimumMemoryMegabytes);
        case MaxRam:return std::to_wstring(draft_.maximumMemoryMegabytes);
        case Width:return std::to_wstring(draft_.windowWidth);
        case Height:return std::to_wstring(draft_.windowHeight);
        case JavaPath:return draft_.javaExecutable.wstring();
        case GamePath:return draft_.gameDirectory.wstring();
        default:return {};
        }
    }
    const int length=GetWindowTextLengthW(control(id));
    std::vector<wchar_t> buffer(static_cast<std::size_t>(length)+1);
    GetWindowTextW(control(id),buffer.data(),length+1);return buffer.data();
}
void SettingsPage::setMessage(std::wstring message,bool error)
{
    message_=std::move(message);error_=error;redraw();
}
void SettingsPage::redraw() const
{
    if(parent_ && visible_) SendMessage(parent_, LauncherConstants::InterfaceRedrawMessage, 0, 0);
}
void SettingsPage::invalidateButtons() const
{
    for (int id=TabGeneral; id<ControlEnd; ++id)
        if (!isEdit(id) && control(id)) InvalidateRect(control(id), nullptr, FALSE);
}
bool SettingsPage::collect(LauncherSettings& settings)
{
    if(locked_) {setMessage(L"Aguarde a instalação ou o jogo encerrar para alterar as preferências.",true);return false;}
    settings=draft_;
    const auto number=[&](int id,int& result) {
        const auto value=text(id);
        if(value.empty() || value.size()>5 || !std::all_of(value.begin(),value.end(),[](wchar_t c) {return c>=L'0' && c<=L'9';})) {
            setMessage(L"Preencha os campos numéricos com valores inteiros.",true);
            if(tabOf(id)!=tab_) setTab(tabOf(id));
            SetFocus(control(id));return false;
        }
        result=std::stoi(value);return true;
    };
    if(!number(MinRam,settings.minimumMemoryMegabytes) || !number(MaxRam,settings.maximumMemoryMegabytes) ||
       !number(Width,settings.windowWidth) || !number(Height,settings.windowHeight)) return false;
    settings.javaExecutable=automaticJava_ ? std::filesystem::path{} : std::filesystem::path(text(JavaPath));
    settings.gameDirectory=text(GamePath);
    if(!automaticJava_ && settings.javaExecutable.empty()) {
        setMessage(L"Escolha o executável Java 17 ou ative Java automático.",true);
        setTab(Tab::Advanced);SetFocus(control(JavaPath));return false;
    }
    std::wstring error;
    if(!LauncherSettingsStore::validate(settings,error)) {setMessage(error,true);return false;}
    return true;
}
void SettingsPage::handle(int id,int notification)
{
    if (id==MaxRam && notification==EN_CHANGE && !updatingMemory_ && !locked_) {
        const auto value=memoryNumber(text(MaxRam));
        if (value && *value>=512 && *value<=65536) {
            syncMemoryControls(*value);setLocked(locked_);redraw();
        }
        return;
    }
    if(notification!=BN_CLICKED) { if(notification==EN_SETFOCUS || notification==EN_KILLFOCUS) redraw();return; }
    switch(id) {
    case TabGeneral:setTab(Tab::General);setLocked(locked_);return;
    case TabAdvanced:setTab(Tab::Advanced);setLocked(locked_);return;
    case TabAbout:setTab(Tab::About);setLocked(locked_);return;
    case OpenChangelog:
        ShellExecuteW(parent_,L"open",L"https://github.com/ryanbworks/Eternal-Launcher/blob/master/CHANGELOG.md",nullptr,nullptr,SW_SHOWNORMAL);return;
    case OpenRepository:
        ShellExecuteW(parent_,L"open",L"https://github.com/ryanbworks/Eternal-Launcher",nullptr,nullptr,SW_SHOWNORMAL);return;
    case OpenDataFolder:openFolder(LauncherSettingsStore::dataDirectory());return;
    default:break;
    }
    if(locked_ && id!=OpenGame && id!=OpenMods && id!=OpenLogs && id!=Cancel) return;
    switch(id) {
    case Ram4:selectMemory(4096);return;
    case Ram6:selectMemory(6144);return;
    case Ram8:selectMemory(8192);return;
    case Fullscreen:draft_.fullscreen=!draft_.fullscreen;break;
    case Minimize:draft_.minimizeOnLaunch=!draft_.minimizeOnLaunch;break;
    case JavaAuto:automaticJava_=true;break;
    case JavaCustom:automaticJava_=false;break;
    case Restore:populate(LauncherSettings{});setMessage(L"Padrões restaurados no formulário. Clique em Salvar para aplicar.");break;
    case Cancel:populate(saved_);setMessage(L"Alterações descartadas.");break;
    case JavaBrowse:browseJava();break;
    case GameBrowse:browseFolder();break;
    case OpenGame:openFolder(LauncherSettingsStore::effectiveGameDirectory(saved_));break;
    case OpenMods:openFolder(LauncherSettingsStore::effectiveGameDirectory(saved_)/L"mods");break;
    case OpenLogs:openFolder(LauncherSettingsStore::effectiveGameDirectory(saved_)/L"logs");break;
    default:break;
    }
    setLocked(locked_);redraw();
}
void SettingsPage::browseJava()
{
    std::array<wchar_t,32768> file{};
    OPENFILENAMEW dialog{sizeof(OPENFILENAMEW)};
    dialog.hwndOwner=parent_;dialog.lpstrFilter=L"Java (java.exe; javaw.exe)\0java.exe;javaw.exe\0Executáveis\0*.exe\0\0";
    dialog.lpstrFile=file.data();dialog.nMaxFile=static_cast<DWORD>(file.size());
    dialog.lpstrTitle=L"Escolher executável Java 17";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetOpenFileNameW(&dialog)) SetWindowTextW(control(JavaPath),file.data());
}
void SettingsPage::browseFolder()
{
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)) {setMessage(L"Não foi possível abrir o seletor de pastas. Digite o caminho no campo.",true);return;}
    IFileDialog* dialog=nullptr;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
        DWORD options=0;
        dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR);
        dialog->SetTitle(L"Escolher pasta exclusiva do Eternal");
        if(SUCCEEDED(dialog->Show(parent_))) {
            IShellItem* item=nullptr;
            if(SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR value=nullptr;
                if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&value))) {
                    SetWindowTextW(control(GamePath),value);CoTaskMemFree(value);
                }
                item->Release();
            }
        }
        dialog->Release();
    } else setMessage(L"Não foi possível abrir o seletor de pastas. Digite o caminho no campo.",true);
    CoUninitialize();
}
void SettingsPage::openFolder(const std::filesystem::path& path)
{
    std::error_code ec;
    if(!std::filesystem::is_directory(path,ec)) {
        setMessage(L"Essa pasta ainda não existe. Ela será preparada ao instalar ou iniciar o jogo.");return;
    }
    if(reinterpret_cast<INT_PTR>(ShellExecuteW(parent_,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
        setMessage(L"Não foi possível abrir a pasta no Explorador.",true);
}
bool SettingsPage::advance(float milliseconds)
{
    const float targets[]={draft_.fullscreen ? 1.0F : 0.0F,draft_.minimizeOnLaunch ? 1.0F : 0.0F};
    bool moving=false;
    for(std::size_t i=0;i<knobs_.size();++i) {
        const float step=milliseconds/200;
        knobs_[i]=knobs_[i]<targets[i] ? std::min(targets[i],knobs_[i]+step) : std::max(targets[i],knobs_[i]-step);
        moving|=knobs_[i]!=targets[i];
    }
    const float tab=static_cast<float>(static_cast<int>(tab_));
    tabIndicator_+=(tab-tabIndicator_)*(1-std::exp(-milliseconds/60));
    if(std::abs(tab-tabIndicator_)<.01F) tabIndicator_=tab;
    moving|=tabIndicator_!=tab;
    return moving;
}
void SettingsPage::settle()
{
    knobs_={draft_.fullscreen ? 1.0F : 0.0F,draft_.minimizeOnLaunch ? 1.0F : 0.0F};
    tabIndicator_=static_cast<float>(static_cast<int>(tab_));
}
int SettingsPage::focusedControlId() const
{
    const auto focus=GetFocus();
    for(int id=TabGeneral;id<ControlEnd;++id) if(focus==control(id)) return id;
    return 0;
}
LRESULT CALLBACK SettingsPage::controlProcedure(HWND child,UINT message,WPARAM wParam,LPARAM lParam,UINT_PTR id,DWORD_PTR data)
{
    auto* page=reinterpret_cast<SettingsPage*>(data);
    // Preserve native EDIT erasure; only the fully owner-drawn buttons suppress it.
    if (message == WM_ERASEBKGND && !isEdit(static_cast<int>(id))) return 1;
    if (id==RamSlider && (message==WM_PAINT || message==WM_PRINTCLIENT)) {
        PAINTSTRUCT paint{};
        const bool begin=message==WM_PAINT && wParam==0;
        const HDC dc=begin ? BeginPaint(child,&paint) : reinterpret_cast<HDC>(wParam);
        DRAWITEMSTRUCT item{};
        item.CtlType=ODT_BUTTON;item.CtlID=RamSlider;item.hwndItem=child;item.hDC=dc;
        GetClientRect(child,&item.rcItem);
        SendMessage(page->parent_,WM_DRAWITEM,RamSlider,reinterpret_cast<LPARAM>(&item));
        if (begin) EndPaint(child,&paint);
        return 0;
    }
    if(message==WM_MOUSEMOVE && !page->hover_[id-TabGeneral]) {
        page->hover_[id-TabGeneral]=true;
        TRACKMOUSEEVENT event{sizeof(TRACKMOUSEEVENT),TME_LEAVE,child,0};TrackMouseEvent(&event);page->redraw();
    } else if(message==WM_MOUSELEAVE) {page->hover_[id-TabGeneral]=false;page->redraw();}
    else if(message==WM_SETCURSOR && !isEdit(static_cast<int>(id)) && IsWindowEnabled(child)) {
        SetCursor(LoadCursor(nullptr,IDC_HAND));return TRUE;
    } else if(message==WM_NCDESTROY) RemoveWindowSubclass(child,controlProcedure,id);
    const auto result=DefSubclassProc(child,message,wParam,lParam);
    if(message==WM_SETFOCUS || message==WM_KILLFOCUS || message==WM_LBUTTONUP ||
       message==WM_LBUTTONDOWN || message==WM_KEYDOWN || message==WM_KEYUP) page->redraw();
    return result;
}
void SettingsPage::drawMemorySlider(Gdiplus::Graphics& g,const Gdiplus::RectF& b,bool focused) const
{
    float start=b.X+12,end=b.GetRight()-12,y=b.Y+b.Height/2;
    const float steps=static_cast<float>(sliderMaximumSteps_-1);
    float knob=start+(end-start)*std::clamp((draft_.maximumMemoryMegabytes/512.0F-1)/steps,0.0F,1.0F);
    if (control(RamSlider)) {
        RECT channel{},thumb{};
        SendMessage(control(RamSlider),TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));
        SendMessage(control(RamSlider),TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
        if (channel.right>channel.left && thumb.right>thumb.left) {
            start=b.X+channel.left/controlScale_;
            end=b.X+channel.right/controlScale_;
            knob=b.X+(thumb.left+thumb.right)/(2*controlScale_);
            y=b.Y+(thumb.top+thumb.bottom)/(2*controlScale_);
        }
    }
    DrawingUtils::fillRoundedRectangle(g,{start,y-3,end-start,6},3,Theme::Border);
    DrawingUtils::fillRoundedRectangle(g,{start,y-3,std::max(1.0F,knob-start),6},3,
        locked_ ? Theme::TextMuted : Theme::Accent);
    Gdiplus::SolidBrush thumb(locked_ ? Theme::TextMuted : hover_[RamSlider-TabGeneral] || focused ? Theme::AccentLight : Theme::Accent);
    g.FillEllipse(&thumb,knob-10,y-10,20.0F,20.0F);
    if (focused) DrawingUtils::drawRoundedRectangle(g,{b.X+1,b.Y+1,b.Width-2,b.Height-2},8,Theme::AccentLight,2);
}
void SettingsPage::draw(Gdiplus::Graphics& g,bool preview) const
{
    using namespace DrawingUtils;
    drawDisplayText(g,L"Configurações",{100,84,600,40},28,Theme::TextPrimary,Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
    drawText(g,L"Deixe o Minecraft do seu jeito. As alterações valem a partir do próximo Jogar.",{100,124,1000,22},14,
        Gdiplus::FontStyleRegular,Theme::TextSecondary);
    Gdiplus::Pen divider(Theme::Divider);
    g.DrawLine(&divider,100.0F,204.0F,1256.0F,204.0F);
    g.DrawLine(&divider,100.0F,712.0F,1256.0F,712.0F);
    const int focus=preview ? 0 : focusedControlId();
    const auto visual=[&](int id) {
        const bool enabled=control(id) ? IsWindowEnabled(control(id))!=FALSE :
            !locked_ && (!(id==JavaPath || id==JavaBrowse) || !automaticJava_) && (!(id==Width || id==Height) || !draft_.fullscreen);
        const bool pressed=!preview && control(id) && (SendMessage(control(id),BM_GETSTATE,0,0)&BST_PUSHED)!=0;
        return ControlVisual{hover_[id-TabGeneral] && enabled && !preview ? 1.0F : 0.0F,focus==id,pressed,enabled};
    };
    const auto edit=[&](int id,std::wstring_view placeholder) {
        const auto b=bounds(id);
        const auto v=visual(id);
        fillRoundedRectangle(g,b,Theme::ControlRadius,v.enabled ? Theme::SurfaceElevated : Theme::SurfaceSunken);
        drawRoundedRectangle(g,b,Theme::ControlRadius,focus==id ? Theme::AccentLight : Theme::Border,focus==id ? 2.0F : 1.0F);
        if(preview) {
            auto value=text(id);
            if(value.empty()) value=std::wstring(placeholder);
            drawText(g,value,{b.X+12,b.Y,b.Width-24,b.Height},14,Gdiplus::FontStyleRegular,
                v.enabled ? Theme::TextPrimary : Theme::TextMuted,Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
        }
    };
    const auto label=[&](std::wstring_view text,float x,float y,float width=248) {
        drawText(g,text,{x,y,width,18},12.5F,Gdiplus::FontStyleBold,Theme::TextSecondary);
    };
    const auto toggleRow=[&](int id,float on,std::wstring_view title,std::wstring_view hint) {
        const auto b=bounds(id);
        const auto v=visual(id);
        if(v.hover>0) fillRoundedRectangle(g,{b.X-8,b.Y,b.Width+16,b.Height},12,Theme::SurfaceSunken);
        drawText(g,title,{b.X,b.Y+5,b.Width-70,20},14,Gdiplus::FontStyleRegular,v.enabled ? Theme::TextPrimary : Theme::TextMuted);
        drawText(g,hint,{b.X,b.Y+26,b.Width-70,18},11.5F,Gdiplus::FontStyleRegular,Theme::TextMuted);
        Buttons::drawSwitch(g,{b.GetRight()-44,b.Y+11,44,26},on,ControlVisual{v.hover,false,false,v.enabled});
        if(v.focused) drawFocusRing(g,{b.X-8,b.Y,b.Width+16,b.Height},12,Theme::AccentLight);
    };

    Buttons::draw(g,bounds(TabGeneral),L"Geral",visual(TabGeneral),Buttons::Style::Tab,std::nullopt,tab_==Tab::General,14);
    Buttons::draw(g,bounds(TabAdvanced),L"Avançado",visual(TabAdvanced),Buttons::Style::Tab,std::nullopt,tab_==Tab::Advanced,14);
    Buttons::draw(g,bounds(TabAbout),L"Sobre",visual(TabAbout),Buttons::Style::Tab,std::nullopt,tab_==Tab::About,14);
    {
        // The underline glides between tabs and stretches to each tab's width.
        const Gdiplus::RectF tabs[]={bounds(TabGeneral),bounds(TabAdvanced),bounds(TabAbout)};
        const float position=std::clamp(tabIndicator_,0.0F,2.0F);
        const int index=std::min(1,static_cast<int>(position));
        const float t=position-static_cast<float>(index);
        const auto& from=tabs[index];const auto& to=tabs[index+1];
        const float x=from.X+(to.X-from.X)*t+8,width=from.Width+(to.Width-from.Width)*t-16;
        fillPill(g,{x,202,width,3},Theme::Accent);
    }

    if(tab_==Tab::General) {
        const auto total=LauncherSettingsStore::physicalMemoryMegabytes();
        card(g,MemoryCard,L"Memória do jogo",total ? L"Seu PC tem "+memoryLabel(static_cast<int>(total/512*512))+L". Deixe pelo menos 4 GB para o Windows." :
            L"Quanto de memória o Minecraft pode usar.");
        drawDisplayText(g,memoryLabel(draft_.maximumMemoryMegabytes),{MemoryCard.GetRight()-184,MemoryCard.Y+20,160,40},30,
            Theme::AccentLight,Gdiplus::StringAlignmentFar);
        label(L"RAM máxima",124,314);
        drawMemorySlider(g,bounds(RamSlider),focus==RamSlider);
        drawText(g,L"0,5 GB",{124,372,90,16},11,Gdiplus::FontStyleRegular,Theme::TextMuted);
        drawText(g,memoryLabel(sliderMaximumSteps_*512),{554,372,90,16},11,Gdiplus::FontStyleRegular,Theme::TextMuted,Gdiplus::StringAlignmentFar);
        const int recommended=recommendedMemory();
        for(const int id:{Ram4,Ram6,Ram8}) {
            const int value=id==Ram4 ? 4096 : id==Ram6 ? 6144 : 8192;
            const auto b=bounds(id);
            const auto v=visual(id);
            const bool selected=draft_.maximumMemoryMegabytes==value;
            Buttons::draw(g,b,L"",v,Buttons::Style::Chip,std::nullopt,selected);
            const bool suggestion=value==recommended;
            drawText(g,memoryLabel(value),{b.X,b.Y+(suggestion ? 8.0F : 0.0F),b.Width,suggestion ? 22.0F : b.Height},14,Gdiplus::FontStyleBold,
                v.enabled ? Theme::TextPrimary : Theme::TextMuted,Gdiplus::StringAlignmentCenter,Gdiplus::StringAlignmentCenter);
            if(suggestion) drawText(g,L"Recomendado",{b.X,b.Y+29,b.Width,18},11,Gdiplus::FontStyleBold,Theme::AccentLight,
                Gdiplus::StringAlignmentCenter,Gdiplus::StringAlignmentCenter);
        }
        card(g,WindowCard,L"Janela do jogo",L"Como o Minecraft abre e o que o launcher faz enquanto você joga.");
        label(L"Largura · pixels",712,316);label(L"Altura · pixels",984,316);
        edit(Width,L"1280");edit(Height,L"720");
        toggleRow(Fullscreen,knobs_[0],L"Abrir em tela cheia",L"Ignora a largura e a altura acima.");
        toggleRow(Minimize,knobs_[1],L"Esconder na bandeja ao jogar",L"O ícone perto do relógio conta seu tempo de jogo.");
    } else if(tab_==Tab::Advanced) {
        card(g,ExactCard,L"Memória exata",L"Valores em MiB (1 GB = 1024 MiB). A RAM inicial não pode passar da máxima.");
        label(L"RAM inicial · MiB",124,324);label(L"RAM máxima · MiB",396,324);
        edit(MinRam,L"512");edit(MaxRam,L"4096");
        card(g,FolderCard,L"Pasta da instância",L"Vazio usa a pasta automática. Trocar a pasta prepara outra instância;\nseus mundos continuam na anterior.");
        label(L"Pasta",124,490,400);
        edit(GamePath,L"Automática: %APPDATA%\\.eternal-launcher\\minecraft");
        Buttons::draw(g,bounds(GameBrowse),L"Escolher",visual(GameBrowse),Buttons::Style::Secondary,std::nullopt,false,12.5F);
        label(L"Atalhos da instância salva",124,604,400);
        Buttons::draw(g,bounds(OpenGame),L"Abrir jogo",visual(OpenGame),Buttons::Style::Ghost,Icons::Icon::Folder,false,12.5F);
        Buttons::draw(g,bounds(OpenMods),L"Abrir mods",visual(OpenMods),Buttons::Style::Ghost,Icons::Icon::Folder,false,12.5F);
        Buttons::draw(g,bounds(OpenLogs),L"Abrir logs",visual(OpenLogs),Buttons::Style::Ghost,Icons::Icon::Folder,false,12.5F);
        card(g,JavaCard,L"Java",L"Automático detecta o Java 17 e prepara um runtime próprio quando necessário.");
        Buttons::draw(g,bounds(JavaAuto),L"Java automático",visual(JavaAuto),Buttons::Style::Chip,std::nullopt,automaticJava_);
        Buttons::draw(g,bounds(JavaCustom),L"Java personalizado",visual(JavaCustom),Buttons::Style::Chip,std::nullopt,!automaticJava_);
        label(L"Executável Java 17",712,386,400);
        edit(JavaPath,automaticJava_ ? L"Java 17 gerenciado pelo Eternal" : L"Escolha java.exe ou javaw.exe");
        Buttons::draw(g,bounds(JavaBrowse),L"Escolher",visual(JavaBrowse),Buttons::Style::Secondary,std::nullopt,false,12.5F);
        drawText(g,automaticJava_ ? L"O launcher prepara o runtime automaticamente quando necessário." :
            L"Use uma instalação Java 17 com java.exe; ela também prepara o Forge.",{712,462,520,40},12.5F,
            Gdiplus::FontStyleRegular,Theme::TextMuted);
    } else {
        card(g,AboutCard,L"",L"");
        emblem_.draw(g,{120,248,52,52});
        drawDisplayText(g,L"Eternal Launcher",{182,250,460,30},22,Theme::TextPrimary);
        drawText(g,L"Versão "+std::wstring(LauncherConfig::LauncherVersion),{182,282,460,20},13,Gdiplus::FontStyleRegular,Theme::AccentLight);
        drawText(g,L"Minecraft "+std::wstring(LauncherConfig::MinecraftVersion)+L" · Forge "+std::wstring(LauncherConfig::ForgeVersion),
            {124,326,520,20},13.5F,Gdiplus::FontStyleRegular,Theme::TextSecondary);
        drawText(g,L"Dados: "+LauncherSettingsStore::dataDirectory().wstring(),{124,352,520,40},12.5F,Gdiplus::FontStyleRegular,Theme::TextMuted);
        drawText(g,L"Atualizações chegam sozinhas: o launcher confere a versão ao abrir e pergunta antes de baixar.",
            {124,392,520,40},12.5F,Gdiplus::FontStyleRegular,Theme::TextMuted);
        Buttons::draw(g,bounds(OpenChangelog),L"Ver changelog",visual(OpenChangelog),Buttons::Style::Ghost,Icons::Icon::External,false,12.5F);
        Buttons::draw(g,bounds(OpenRepository),L"Repositório no GitHub",visual(OpenRepository),Buttons::Style::Ghost,Icons::Icon::External,false,12.5F);
        Buttons::draw(g,bounds(OpenDataFolder),L"Pasta de dados",visual(OpenDataFolder),Buttons::Style::Ghost,Icons::Icon::Folder,false,12.5F);
        card(g,CreditsCard,L"Créditos",L"Feito para o grupo Eternal.");
        drawText(g,L"Desenvolvimento: Ryan (ryanbworks)\nCorreções: Gabriel Henrique (GabrielHen-dev)\n\n"
            L"Fonte Sora — SIL Open Font License\nJava: Eclipse Temurin 17 (Adoptium)\nMinecraft Forge 47.4.10\n\n"
            L"Minecraft é uma marca da Mojang Studios. Este launcher não é afiliado à Mojang ou à Microsoft.",
            {712,312,520,300},13,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    }

    const auto message=locked_ ? L"Alterações bloqueadas durante instalação, login ou execução do jogo." : std::wstring_view(message_);
    drawText(g,message,{100,726,740,46},12.5F,Gdiplus::FontStyleRegular,error_ && !locked_ ? Theme::Danger : Theme::TextSecondary,
        Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
    Buttons::draw(g,bounds(Restore),L"Restaurar padrões",visual(Restore),Buttons::Style::Ghost,std::nullopt,false,13);
    Buttons::draw(g,bounds(Cancel),L"Descartar",visual(Cancel),Buttons::Style::Ghost,std::nullopt,false,13);
    Buttons::draw(g,bounds(Save),L"Salvar",visual(Save),Buttons::Style::Primary,std::nullopt,false,13);
}
