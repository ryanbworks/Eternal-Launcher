#pragma once
#include <windows.h>
#include <stop_token>

struct InstallationProcessResult { bool success=false;bool cancelled=false;DWORD exitCode=1;DWORD error=0; };
// Shared by tar and Forge: callers retain ownership of the process handle.
inline InstallationProcessResult waitForInstallationProcess(HANDLE process,std::stop_token stop) {
    InstallationProcessResult result;
    for(;;) {
        if(stop.stop_requested()) {
            result.cancelled=true;
            if(!TerminateProcess(process,ERROR_CANCELLED) && WaitForSingleObject(process,0)!=WAIT_OBJECT_0) {
                result.error=GetLastError();return result;
            }
            WaitForSingleObject(process,INFINITE);break;
        }
        const auto wait=WaitForSingleObject(process,100);
        if(wait==WAIT_OBJECT_0) break;
        if(wait!=WAIT_TIMEOUT) {result.error=GetLastError();return result;}
    }
    if(!GetExitCodeProcess(process,&result.exitCode)) {result.error=GetLastError();return result;}
    result.cancelled |= stop.stop_requested();
    result.success=!result.cancelled && result.exitCode==0;
    return result;
}
