#include "pages/SettingsPage.h"
#include <windows.h>
#include <iostream>

namespace {
    int failures=0;
    void check(bool okay,const char* message) {if(!okay) {std::cerr << message << '\n';++failures;}}
    void field(HWND parent,int id,const wchar_t* value) {SetWindowTextW(GetDlgItem(parent,id),value);}
}
int main() {
    // Hidden test-owned host: exercise real native EDIT/BUTTON controls without desktop interaction.
    const auto instance=GetModuleHandleW(nullptr);
    const auto parent=CreateWindowExW(0,L"STATIC",L"Settings test host",WS_OVERLAPPEDWINDOW,
        0,0,1180,730,nullptr,nullptr,instance,nullptr);
    if(!parent) return 2;
    {
        SettingsPage page;
        check(page.create(parent,instance),"settings controls must be created");
        page.layout(1,0,0);page.open(LauncherSettings{},{});
        LauncherSettings result;
        check(page.collect(result) && result.maximumMemoryMegabytes==4096 && result.minimizeOnLaunch,
            "native form must load defaults with minimize on launch enabled");
        check(!IsWindowEnabled(GetDlgItem(parent,SettingsPage::JavaPath)),"automatic Java disables path editing");
        field(parent,SettingsPage::MinRam,L"1024");field(parent,SettingsPage::MaxRam,L"6144");
        field(parent,SettingsPage::Width,L"1600");field(parent,SettingsPage::Height,L"900");
        page.handle(SettingsPage::Fullscreen,BN_CLICKED);page.handle(SettingsPage::Minimize,BN_CLICKED);
        check(!IsWindowEnabled(GetDlgItem(parent,SettingsPage::Width)),"fullscreen disables window dimensions");
        check(page.collect(result) && result.minimumMemoryMegabytes==1024 && result.maximumMemoryMegabytes==6144 &&
            result.windowWidth==1600 && result.windowHeight==900 && result.fullscreen && !result.minimizeOnLaunch,
            "edited native fields and toggles must reach collected settings");
        page.setLocked(true);
        check(!IsWindowEnabled(GetDlgItem(parent,SettingsPage::Save)) && !page.collect(result),"busy form must refuse saving");
        check(IsWindowEnabled(GetDlgItem(parent,SettingsPage::Back)) && IsWindowEnabled(GetDlgItem(parent,SettingsPage::OpenLogs)),
            "read-only form must allow returning and reading logs");
        page.setLocked(false);page.handle(SettingsPage::JavaCustom,BN_CLICKED);
        check(IsWindowEnabled(GetDlgItem(parent,SettingsPage::JavaPath)),"custom Java enables selection");
        check(!page.collect(result),"empty custom Java must not silently choose automatic mode");
        page.handle(SettingsPage::JavaAuto,BN_CLICKED);
        field(parent,SettingsPage::MinRam,L"8000");field(parent,SettingsPage::MaxRam,L"1024");
        check(!page.collect(result),"native form must reject reversed memory bounds");
        field(parent,SettingsPage::MinRam,L"1.5");
        check(!page.collect(result),"pasted fractional input must be rejected");
        page.handle(SettingsPage::Restore,BN_CLICKED);
        check(page.collect(result) && result.minimumMemoryMegabytes==512 && result.maximumMemoryMegabytes==4096 &&
            !result.fullscreen && result.minimizeOnLaunch && result.javaExecutable.empty(),"restore must reset form fields and modes");
        field(parent,SettingsPage::MaxRam,L"6000");page.open(LauncherSettings{},{});
        check(page.collect(result) && result.maximumMemoryMegabytes==4096,"reopening without saving must discard edits");
    }
    DestroyWindow(parent);
    std::cout << "Native settings form failures: " << failures << '\n';
    return failures ? 1 : 0;
}
