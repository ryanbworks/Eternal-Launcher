#include "services/HttpDownloadClient.h"
#include "config/LauncherVersion.h"
#include <windows.h>
#include <winhttp.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <fstream>

namespace {
    struct InternetHandle {
        HINTERNET value=nullptr;
        ~InternetHandle() { if(value) WinHttpCloseHandle(value); }
    };
    // Only the calling worker invokes WinHTTP APIs. Callbacks live until HANDLE_CLOSING.
    struct Request {
        HANDLE ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        HANDLE closed=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        HINTERNET handle=nullptr;
        bool callbackInstalled=false;
        std::atomic<DWORD> completion{0},error{0},readBytes{0},availableBytes{0};
        ~Request() {
            close();
            if(ready) CloseHandle(ready);
            if(closed) CloseHandle(closed);
        }
        void close() {
            if(!handle) return;
            const auto old=handle; handle=nullptr;
            WinHttpCloseHandle(old);
            if(callbackInstalled) WaitForSingleObject(closed,INFINITE);
        }
        static void CALLBACK callback(HINTERNET,DWORD_PTR context,DWORD status,void* data,DWORD bytes) {
            const auto self=reinterpret_cast<Request*>(context);
            if(!self) return;
            if(status==WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) { SetEvent(self->closed); return; }
            if(status==WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
                self->error=static_cast<WINHTTP_ASYNC_RESULT*>(data)->dwError;
                SetEvent(self->ready); return;
            }
            if(status==WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE || status==WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE ||
               status==WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE || status==WINHTTP_CALLBACK_STATUS_READ_COMPLETE) {
                if(status==WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE) self->availableBytes=*static_cast<DWORD*>(data);
                if(status==WINHTTP_CALLBACK_STATUS_READ_COMPLETE) self->readBytes=bytes;
                self->completion=status; SetEvent(self->ready);
            }
        }
        void reset() {completion=0;error=0;readBytes=0;ResetEvent(ready);}
        bool wait(DWORD expected,std::stop_token stop,DWORD timeout) {
            const auto deadline=GetTickCount64()+timeout;
            while(!stop.stop_requested()) {
                if(error) return false;
                if(completion==expected) return true;
                if(GetTickCount64()>=deadline) { error=ERROR_WINHTTP_TIMEOUT;return false; }
                WaitForSingleObject(ready,25);
            }
            return false;
        }
    };
    bool pending(BOOL success) { return success || GetLastError()==ERROR_IO_PENDING; }
    void fail(HttpDownloadResult& result,InstallationFailure category,std::wstring_view message,DWORD error=0) {
        result.failure=category;result.errorMessage=message;
        if(error) result.errorMessage+=L" (erro Windows "+std::to_wstring(error)+L").";
    }
    HttpDownloadResult fetch(std::wstring_view url,const std::filesystem::path* destination,
        std::stop_token stop,const HttpDownloadProgressCallback& progress) {
        HttpDownloadResult result;
        if(stop.stop_requested()) {result.cancelled=true;result.errorMessage=L"Download cancelado.";return result;}
        URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
        parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=static_cast<DWORD>(-1);
        const std::wstring ownedUrl(url);
        if(!WinHttpCrackUrl(ownedUrl.c_str(),0,0,&parts) ||
            (parts.nScheme!=INTERNET_SCHEME_HTTP && parts.nScheme!=INTERNET_SCHEME_HTTPS)) {
            fail(result,InstallationFailure::Configuration,L"Endereço de download inválido.");return result;
        }
        InternetHandle session{WinHttpOpen(ETERNAL_USER_AGENT,WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,WINHTTP_FLAG_ASYNC)};
        if(!session.value) {fail(result,InstallationFailure::Network,L"Não foi possível iniciar a conexão.",GetLastError());return result;}
        WinHttpSetTimeouts(session.value,5000,5000,10000,30000);
        const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
        std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);
        if(parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
        InternetHandle connection{WinHttpConnect(session.value,host.c_str(),parts.nPort,0)};
        if(!connection.value) {fail(result,InstallationFailure::Network,L"Não foi possível conectar.",GetLastError());return result;}
        std::array<char,64*1024> buffer{}; // Outlives every callback, including cancellation.
        Request request;
        if(!request.ready || !request.closed) {fail(result,InstallationFailure::Network,L"Não foi possível preparar a conexão.");return result;}
        request.handle=WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,parts.nScheme==INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        if(!request.handle) {fail(result,InstallationFailure::Network,L"Não foi possível preparar o download.",GetLastError());return result;}
        DWORD_PTR context=reinterpret_cast<DWORD_PTR>(&request);
        if(!WinHttpSetOption(request.handle,WINHTTP_OPTION_CONTEXT_VALUE,&context,sizeof(context)) ||
           WinHttpSetStatusCallback(request.handle,Request::callback,WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS|WINHTTP_CALLBACK_FLAG_HANDLES,0)==WINHTTP_INVALID_STATUS_CALLBACK) {
            fail(result,InstallationFailure::Network,L"Não foi possível preparar os eventos de rede.",GetLastError());return result;
        }
        request.callbackInstalled=true;
        const auto networkFailure=[&] {
            result.cancelled=stop.stop_requested();
            fail(result,InstallationFailure::Network,result.cancelled ? L"Download cancelado." : L"A conexão foi interrompida. Confira sua internet e tente novamente.",request.error.load());
        };
        if(!pending(WinHttpSendRequest(request.handle,WINHTTP_NO_ADDITIONAL_HEADERS,0,nullptr,0,0,context)) ||
            !request.wait(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,stop,15000)) {networkFailure();return result;}
        request.reset();
        if(!pending(WinHttpReceiveResponse(request.handle,nullptr)) ||
            !request.wait(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,stop,30000)) {networkFailure();return result;}
        DWORD status=0,size=sizeof(status);
        if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)) {
            networkFailure();return result;
        }
        result.statusCode=status;
        if(status<200 || status>=300) {fail(result,InstallationFailure::Http,L"O servidor respondeu com HTTP "+std::to_wstring(status)+L". Tente novamente mais tarde.");return result;}
        wchar_t length[64]{};size=sizeof(length);
        const std::uint64_t total=WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_CONTENT_LENGTH,nullptr,length,&size,nullptr)
            ? _wcstoui64(length,nullptr,10) : 0;
        std::ofstream output;
        if(destination) {
            std::filesystem::create_directories(destination->parent_path());
            output.open(*destination,std::ios::binary|std::ios::trunc);
            if(!output) {fail(result,InstallationFailure::Disk,L"Não foi possível gravar o download. Confira espaço livre e permissões.");return result;}
        }
        std::uint64_t received=0;
        if(progress) progress(0,total);
        while(!stop.stop_requested()) {
            request.reset();
            if(!pending(WinHttpQueryDataAvailable(request.handle,nullptr)) ||
                !request.wait(WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE,stop,30000)) {networkFailure();break;}
            const auto available=std::min<DWORD>(request.availableBytes,static_cast<DWORD>(buffer.size()));
            if(!available) {result.success=true;break;}
            request.reset();
            if(!pending(WinHttpReadData(request.handle,buffer.data(),available,nullptr)) ||
                !request.wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE,stop,30000)) {networkFailure();break;}
            const DWORD bytes=request.readBytes;
            if(!bytes) {result.success=true;break;}
            if(destination) {
                output.write(buffer.data(),bytes);
                if(!output) {fail(result,InstallationFailure::Disk,L"Falha ao gravar o download. Confira espaço livre e permissões.");break;}
            } else {
                if(result.content.size()+bytes>32*1024*1024) {fail(result,InstallationFailure::Configuration,L"Metadados excedem o tamanho permitido.");break;}
                result.content.append(buffer.data(),bytes);
            }
            received+=bytes;
            if(progress) progress(received,total);
        }
        if(output.is_open()) {output.flush();if(!output) {result.success=false;fail(result,InstallationFailure::Disk,L"Falha ao concluir a gravação do download.");}output.close();}
        if(stop.stop_requested()) {result.success=false;result.cancelled=true;result.errorMessage=L"Download cancelado.";}
        if(result.success && total && received!=total) {result.success=false;fail(result,InstallationFailure::Network,L"O download terminou incompleto. Tente novamente.");}
        if(!destination && result.success && result.content.empty()) {result.success=false;fail(result,InstallationFailure::Configuration,L"O servidor retornou metadados vazios.");}
        return result;
    }
}
HttpDownloadResult HttpDownloadClient::getText(std::wstring_view url,std::stop_token stop) const {
    try { return fetch(url,nullptr,stop,{}); }
    catch(const std::exception&) {HttpDownloadResult r;fail(r,InstallationFailure::Disk,L"Não foi possível preparar os metadados.");return r;}
}
HttpDownloadResult HttpDownloadClient::downloadFile(std::wstring_view url,const std::filesystem::path& destination,
    std::stop_token stop,const HttpDownloadProgressCallback& progress) const {
    HttpDownloadResult result;
    try {result=fetch(url,&destination,stop,progress);}
    catch(const std::exception&) {fail(result,InstallationFailure::Disk,L"Não foi possível gravar o download. Confira espaço livre e permissões.");}
    if(!result.success) {std::error_code ignored;std::filesystem::remove(destination,ignored);}
    return result;
}
