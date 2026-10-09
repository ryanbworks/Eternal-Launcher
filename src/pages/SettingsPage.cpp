#include "pages/SettingsPage.h"
#include "constants/LauncherConstants.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"
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
    const wchar_t* Labels[] = {L"Voltar",L"RAM inicial (MiB)",L"RAM máxima (MiB)",L"Largura (pixels)",L"Altura (pixels)",
        L"Iniciar em tela cheia",L"Minimizar launcher ao jogar",L"Java automático",L"Java personalizado",
        L"Executável Java 17",L"Escolher Java",L"Pasta da instância",L"Escolher pasta",
        L"Abrir jogo",L"Abrir mods",L"Abrir logs",L"Restaurar padrões",L"Cancelar",L"Salvar preferências",
        L"RAM máxima em passos de 0,5 GB",L"4 GB",L"6 GB",L"8 GB"};
    std::optional<int> memoryNumber(std::wstring_view text) {
        if (text.empty() || text.size()>5) return {};
        int value=0;
        for (const auto c:text) {
            if (c<L'0' || c>L'9') return {};
            value=value*10+(c-L'0');
        }
        return value;
    }
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
Gdiplus::RectF SettingsPage::bounds(int id)
{
    switch(id) {
    case Back:return {36,112,96,36};
    case MinRam:return {36,407,232,38};
    case MaxRam:return {292,407,232,38};
    case RamSlider:return {36,300,488,32};
    case Ram4:return {36,348,152,30};
    case Ram6:return {204,348,152,30};
    case Ram8:return {372,348,152,30};
    case Width:return {36,514,232,38};
    case Height:return {292,514,232,38};
    case Fullscreen:return {36,568,488,32};
    case Minimize:return {36,608,488,32};
    case JavaAuto:return {576,264,280,36};
    case JavaCustom:return {860,264,284,36};
    case JavaPath:return {576,320,478,42};
    case JavaBrowse:return {1070,320,74,42};
    case GamePath:return {576,468,478,42};
    case GameBrowse:return {1070,468,74,42};
    case OpenGame:return {576,584,176,40};
    case OpenMods:return {772,584,176,40};
    case OpenLogs:return {968,584,176,40};
    case Restore:return {646,666,178,44};
    case Cancel:return {840,666,116,44};
    case Save:return {972,666,172,44};
    default:return {};
    }
}
bool SettingsPage::create(HWND parent,HINSTANCE instance)
{
    parent_=parent; instance_=instance;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&common)) return false;
    for (int id=Back;id<ControlEnd;++id) {
        const bool edit=isEdit(id);
        const bool slider=id==RamSlider;
        DWORD style=WS_CHILD | WS_TABSTOP | (edit ? ES_AUTOHSCROLL : slider ? TBS_HORZ|TBS_NOTICKS|TBS_FIXEDLENGTH : BS_OWNERDRAW);
        if (edit && id!=JavaPath && id!=GamePath) style |= ES_NUMBER;
        controls_[id-Back]=CreateWindowExW(0,edit ? L"EDIT" : slider ? TRACKBAR_CLASSW : L"BUTTON",edit ? L"" : Labels[id-Back],
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
    const int tabOrder[]={Back,RamSlider,Ram4,Ram6,Ram8,MinRam,MaxRam,Width,Height,Fullscreen,Minimize,
        JavaAuto,JavaCustom,JavaPath,JavaBrowse,GamePath,GameBrowse,OpenGame,OpenMods,OpenLogs,Restore,Cancel,Save};
    HWND previous=HWND_TOP;
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
void SettingsPage::show(bool visible)
{
    visible_=visible;
    for (const auto child:controls_) if(child) ShowWindow(child,visible ? SW_SHOW : SW_HIDE);
    if (visible && control(Back)) SetFocus(control(Back));
}
void SettingsPage::layout(float scale,float ox,float oy)
{
    controlScale_=scale;
    const auto font=CreateFontW(-static_cast<int>(std::round(14*scale)),0,0,0,FW_NORMAL,
        FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,DEFAULT_PITCH | FF_DONTCARE,Theme::FontFamily);
    for(int id=Back;id<ControlEnd;++id) {
        if(!control(id)) continue;
        auto b=bounds(id);
        if(isEdit(id)) {b.X+=12; b.Y+=11; b.Width-=24; b.Height-=18;}
        const int x=static_cast<int>(std::floor(ox+b.X*scale)),y=static_cast<int>(std::floor(oy+b.Y*scale));
        const int r=static_cast<int>(std::ceil(ox+b.GetRight()*scale)),bottom=static_cast<int>(std::ceil(oy+b.GetBottom()*scale));
        layoutBounds_[id-Back]={x,y,r,bottom};
        SetWindowPos(control(id),nullptr,x,y,r-x,bottom-y,SWP_NOZORDER|SWP_NOACTIVATE);
        if(font) SendMessage(control(id),WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    }
    if(font) {if(font_) DeleteObject(font_);font_=font;}
    SendMessage(control(RamSlider),TBM_SETTHUMBLENGTH,static_cast<WPARAM>(std::round(20*scale)),0);
}
void SettingsPage::setTransitionOffset(int pixels)
{
    auto batch=BeginDeferWindowPos(static_cast<int>(controls_.size()));
    for(std::size_t i=0;i<controls_.size();++i) {
        if(!controls_[i]) continue;
        const auto& b=layoutBounds_[i];
        if(batch) batch=DeferWindowPos(batch,controls_[i],nullptr,b.left+pixels,b.top,0,0,
            SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
        if(!batch) break;
    }
    if(batch && EndDeferWindowPos(batch)) return;
    for(std::size_t i=0;i<controls_.size();++i) if(controls_[i])
        SetWindowPos(controls_[i],nullptr,layoutBounds_[i].left+pixels,layoutBounds_[i].top,0,0,
            SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
}
void SettingsPage::setLocked(bool locked)
{
    locked_=locked;
    for(int id=Back;id<ControlEnd;++id) if(control(id)) {
        const bool editable=id==Back || id==Cancel || id==OpenGame || id==OpenMods || id==OpenLogs || !locked;
        const bool javaEditable=!(id==JavaPath || id==JavaBrowse) || !automaticJava_;
        const bool windowEditable=!(id==Width || id==Height) || !draft_.fullscreen;
        const bool enabled=editable && javaEditable && windowEditable;
        if ((IsWindowEnabled(control(id))!=FALSE)!=enabled) EnableWindow(control(id),enabled);
    }
    const auto label=[&](int id,bool selected) {
        const std::wstring value=std::wstring(Labels[id-Back])+(selected ? L" (selecionado)" : L"");
        std::array<wchar_t,128> existing{}; GetWindowTextW(control(id),existing.data(),static_cast<int>(existing.size()));
        if(value!=existing.data()) SetWindowTextW(control(id),value.c_str());
    };
    if(control(JavaAuto)) {
        label(JavaAuto,automaticJava_);label(JavaCustom,!automaticJava_);
        label(Fullscreen,draft_.fullscreen);label(Minimize,draft_.minimizeOnLaunch);
        label(Ram4,draft_.maximumMemoryMegabytes==4096);
        label(Ram6,draft_.maximumMemoryMegabytes==6144);
        label(Ram8,draft_.maximumMemoryMegabytes==8192);
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
    for (int id=Back; id<ControlEnd; ++id)
        if (!isEdit(id) && control(id)) InvalidateRect(control(id), nullptr, FALSE);
}
bool SettingsPage::collect(LauncherSettings& settings)
{
    if(locked_) {setMessage(L"Aguarde a instalação ou o jogo encerrar para alterar as preferências.",true);return false;}
    settings=draft_;
    const auto number=[&](int id,int& result) {
        const auto value=text(id);
        if(value.empty() || value.size()>5 || !std::all_of(value.begin(),value.end(),[](wchar_t c) {return c>=L'0' && c<=L'9';})) {
            setMessage(L"Preencha os campos numéricos com valores inteiros.",true);SetFocus(control(id));return false;
        }
        result=std::stoi(value);return true;
    };
    if(!number(MinRam,settings.minimumMemoryMegabytes) || !number(MaxRam,settings.maximumMemoryMegabytes) ||
       !number(Width,settings.windowWidth) || !number(Height,settings.windowHeight)) return false;
    settings.javaExecutable=automaticJava_ ? std::filesystem::path{} : std::filesystem::path(text(JavaPath));
    settings.gameDirectory=text(GamePath);
    if(!automaticJava_ && settings.javaExecutable.empty()) {
        setMessage(L"Escolha o executável Java 17 ou ative Java automático.",true);SetFocus(control(JavaPath));return false;
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
    if(locked_ && id!=OpenGame && id!=OpenMods && id!=OpenLogs) return;
    switch(id) {
    case Ram4:selectMemory(4096);return;
    case Ram6:selectMemory(6144);return;
    case Ram8:selectMemory(8192);return;
    case Fullscreen:draft_.fullscreen=!draft_.fullscreen;break;
    case Minimize:draft_.minimizeOnLaunch=!draft_.minimizeOnLaunch;break;
    case JavaAuto:automaticJava_=true;break;
    case JavaCustom:automaticJava_=false;break;
    case Restore:populate(LauncherSettings{});setMessage(L"Padrões restaurados no formulário. Clique em Salvar para aplicar.");break;
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
int SettingsPage::focusedControlId() const
{
    const auto focus=GetFocus();
    for(int id=Back;id<ControlEnd;++id) if(focus==control(id)) return id;
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
    if(message==WM_MOUSEMOVE && !page->hover_[id-Back]) {
        page->hover_[id-Back]=true;
        TRACKMOUSEEVENT event{sizeof(TRACKMOUSEEVENT),TME_LEAVE,child,0};TrackMouseEvent(&event);page->redraw();
    } else if(message==WM_MOUSELEAVE) {page->hover_[id-Back]=false;page->redraw();}
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
    Gdiplus::SolidBrush thumb(locked_ ? Theme::TextMuted : hover_[RamSlider-Back] || focused ? Theme::AccentLight : Theme::Accent);
    g.FillEllipse(&thumb,knob-9,y-9,18.0F,18.0F);
    if (focused) DrawingUtils::drawRoundedRectangle(g,{b.X+1,b.Y+1,b.Width-2,b.Height-2},8,Theme::AccentLight,2);
}
void SettingsPage::draw(Gdiplus::Graphics& g,bool preview) const
{
    using namespace DrawingUtils;
    drawText(g,L"Configurações",{148,109,600,40},28,Gdiplus::FontStyleBold,Theme::TextPrimary);
    drawText(g,L"Deixe o Minecraft do seu jeito. As alterações entram no próximo Jogar.",{36,164,1040,26},13,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    Gdiplus::Pen divider(Theme::Border);
    g.DrawLine(&divider,550.0F,208.0F,550.0F,624.0F);
    drawText(g,L"Desempenho",{36,211,488,30},20,Gdiplus::FontStyleBold,Theme::TextPrimary);
    const auto total=LauncherSettingsStore::physicalMemoryMegabytes();
    drawText(g,total ? L"RAM instalada: "+std::to_wstring(total)+L" MiB. Reserve memória para o Windows." :
        L"Defina a memória reservada para o Minecraft.",{36,246,488,23},12,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    drawText(g,L"RAM máxima · passos de 0,5 GB",{36,275,330,22},12,Gdiplus::FontStyleBold,Theme::TextSecondary);
    drawText(g,memoryLabel(draft_.maximumMemoryMegabytes),{388,270,136,28},18,Gdiplus::FontStyleBold,
        Theme::AccentLight,Gdiplus::StringAlignmentFar);
    drawText(g,L"0,5 GB",{36,332,90,16},10,Gdiplus::FontStyleRegular,Theme::TextMuted);
    drawText(g,memoryLabel(sliderMaximumSteps_*512),{434,332,90,16},10,Gdiplus::FontStyleRegular,
        Theme::TextMuted,Gdiplus::StringAlignmentFar);
    drawText(g,L"RAM inicial · MiB",{36,385,232,20},12,Gdiplus::FontStyleBold,Theme::TextSecondary);
    drawText(g,L"RAM máxima · MiB (1 GB = 1024)",{292,385,232,20},11,Gdiplus::FontStyleBold,Theme::TextSecondary);
    drawText(g,L"Janela do jogo",{36,460,488,28},20,Gdiplus::FontStyleBold,Theme::TextPrimary);
    drawText(g,L"Largura · pixels",{36,491,232,20},12,Gdiplus::FontStyleBold,Theme::TextSecondary);
    drawText(g,L"Altura · pixels",{292,491,232,20},12,Gdiplus::FontStyleBold,Theme::TextSecondary);
    drawText(g,L"Java",{576,211,568,28},20,Gdiplus::FontStyleBold,Theme::TextPrimary);
    drawText(g,L"Automático detecta o Java 17 e prepara o runtime quando necessário.",{576,246,568,20},12,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    drawText(g,L"Executável Java 17",{576,301,568,18},11,Gdiplus::FontStyleBold,Theme::TextSecondary);
    drawText(g,automaticJava_ ? L"O launcher prepara o runtime automaticamente quando necessário." :
        L"Use uma instalação Java 17; ela também será usada para preparar Forge.",
        {576,375,568,35},12,Gdiplus::FontStyleRegular,Theme::TextMuted);
    drawText(g,L"Arquivos do jogo",{576,421,568,28},20,Gdiplus::FontStyleBold,Theme::TextPrimary);
    drawText(g,L"Pasta exclusiva da instância · vazio usa a pasta automática",{576,450,568,20},11,Gdiplus::FontStyleBold,Theme::TextSecondary);
    drawText(g,L"Trocar a pasta prepara outra instância. Seus mundos permanecem\nna pasta anterior. Os atalhos abrem a instância já salva.",
        {576,526,568,44},12,Gdiplus::FontStyleRegular,Theme::TextMuted);
    g.DrawLine(&divider,36.0F,646.0F,1144.0F,646.0F);
    const auto message=locked_ ? L"Alterações bloqueadas durante instalação, login ou execução do jogo." : std::wstring_view(message_);
    drawText(g,message,{36,666,582,48},12,Gdiplus::FontStyleRegular,error_ && !locked_ ? Theme::Error : Theme::TextSecondary);
    const int focus=preview ? 0 : focusedControlId();
    for(int id=Back;id<ControlEnd;++id) {
        const auto b=bounds(id);
        const bool enabled=control(id) ? IsWindowEnabled(control(id))!=FALSE :
            !locked_ && (!(id==JavaPath || id==JavaBrowse) || !automaticJava_);
        if (id==RamSlider) { drawMemorySlider(g,b,focus==id); continue; }
        if(isEdit(id)) {
            fillRoundedRectangle(g,b,Theme::ControlRadius,Theme::SurfaceElevated);
            drawRoundedRectangle(g,b,Theme::ControlRadius,focus==id ? Theme::AccentLight : Theme::Border,focus==id ? 2.0F : 1.0F);
            if(preview) {
                auto value=text(id);
                if(value.empty()) value=id==GamePath ? L"Automática: %APPDATA%\\.eternal-launcher\\minecraft" : L"Java 17 gerenciado pelo Eternal";
                drawText(g,value,{b.X+12,b.Y,b.Width-24,b.Height},14,Gdiplus::FontStyleRegular,
                    enabled ? Theme::TextPrimary : Theme::TextMuted,Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
            }
            continue;
        }
        const bool checked=(id==Fullscreen && draft_.fullscreen) || (id==Minimize && draft_.minimizeOnLaunch);
        const bool selected=(id==JavaAuto && automaticJava_) || (id==JavaCustom && !automaticJava_) ||
            (id==Ram4 && draft_.maximumMemoryMegabytes==4096) || (id==Ram6 && draft_.maximumMemoryMegabytes==6144) ||
            (id==Ram8 && draft_.maximumMemoryMegabytes==8192);
        const bool hovered=hover_[id-Back] && enabled;
        const bool pressed=control(id) && (SendMessage(control(id),BM_GETSTATE,0,0)&BST_PUSHED)!=0;
        const bool primary=id==Save;
        auto fill=primary ? (enabled ? Theme::Accent : Theme::SurfaceHover) :
            selected || hovered || pressed ? Theme::SurfaceHover : Theme::Surface;
        if(primary && (hovered || pressed)) fill=Theme::AccentLight;
        fillRoundedRectangle(g,b,Theme::ControlRadius,fill);
        const auto ink=primary && enabled ? Theme::AccentInk : enabled ? selected ? Theme::AccentLight : Theme::TextPrimary : Theme::TextMuted;
        if(id==Fullscreen || id==Minimize) {
            const Gdiplus::RectF box(b.X+12,b.Y+(b.Height-18)/2,18,18);
            fillRoundedRectangle(g,box,4,checked ? Theme::Accent : Theme::Background);
            if(checked) {
                Gdiplus::Pen check(Theme::AccentInk,2);
                g.DrawLine(&check,box.X+4,box.Y+9,box.X+8,box.Y+13);
                g.DrawLine(&check,box.X+8,box.Y+13,box.X+14,box.Y+5);
            } else drawRoundedRectangle(g,box,4,Theme::TextMuted);
            drawText(g,Labels[id-Back],{b.X+42,b.Y,b.Width-54,b.Height},13,Gdiplus::FontStyleRegular,ink,
                Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
        } else drawText(g,id==JavaBrowse || id==GameBrowse ? L"Escolher" : Labels[id-Back],b,
            12.0F,Gdiplus::FontStyleBold,ink,Gdiplus::StringAlignmentCenter,Gdiplus::StringAlignmentCenter);
        if(focus==id) drawRoundedRectangle(g,{b.X+2,b.Y+2,b.Width-4,b.Height-4},8,
            primary ? Theme::AccentInk : Theme::AccentLight,2);
    }
}
