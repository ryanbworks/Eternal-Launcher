// Manual release smoke check. This tool is never included in the distribution.
#include "services/LauncherUpdateTransport.h"
#include "services/LauncherUpdateManifest.h"
#include "services/LauncherUpdateService.h"
#include <iostream>
#include <thread>
#include <chrono>
int wmain(int count, wchar_t** args) {
    std::stop_source source;
    LauncherUpdateTransport transport;
    if(count==2 && std::wstring_view(args[1])==L"--cancel") {
        std::jthread cancel([&] {std::this_thread::sleep_for(std::chrono::milliseconds(10));source.request_stop();});
        const auto start=GetTickCount64();
        const auto response=transport.manifest(source.get_token());
        const auto elapsed=GetTickCount64()-start;
        std::cout << "Cancellation completed in " << elapsed << " ms\n";
        return elapsed<2000 && !response.success ? 0 : 1;
    }
    const auto response=transport.manifest(source.get_token());
    if(!response.success) {std::wcerr << response.error << '\n';return 1;}
    std::wstring error;
    const auto release=LauncherUpdateManifest::verify(response.text,error);
    if(!release) {std::wcerr << error << '\n';return 1;}
    std::cout << "Valid signed release: " << release->version << " (" << release->size << " bytes)\n";
    if(count==3 && std::wstring_view(args[1])==L"--download") {
        const auto download=transport.download(release->path,args[2],release->size,source.get_token(),[](auto){});
        if(!download.success || !LauncherUpdateRuntime::validateExecutable(args[2],*release)) return 1;
        std::cout << "Native WinHTTP download and executable verification passed\n";
    }
    return 0;
}
