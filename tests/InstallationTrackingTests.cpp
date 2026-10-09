#include <winsock2.h>
#include <ws2tcpip.h>
#include "types/InstallationProgress.h"
#include "services/HttpDownloadClient.h"
#include "services/VerifiedDownloader.h"
#include "services/InstallationCoordinator.h"
#include "services/GameInstallationService.h"
#include "services/JavaRuntimeService.h"
#include "utils/FileHash.h"
#include "utils/InstallationProcess.h"
#include <atomic>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace {
    int failures=0;
    void check(bool condition,const char* message) {if(!condition) {++failures;std::cerr<<"FAIL: "<<message<<'\n';}}
    void write(const std::filesystem::path& path,std::string_view content) {
        std::filesystem::create_directories(path.parent_path());std::ofstream output(path,std::ios::binary);output<<content;
    }
    std::string read(const std::filesystem::path& path) {
        std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};
    }
    class LocalServer {
    public:
        LocalServer() {
            WSADATA data{};WSAStartup(MAKEWORD(2,2),&data);
            listener_=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
            sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
            if(bind(listener_,reinterpret_cast<sockaddr*>(&address),sizeof(address)) || listen(listener_,16)) throw std::runtime_error("local fixture bind failed");
            int size=sizeof(address);getsockname(listener_,reinterpret_cast<sockaddr*>(&address),&size);
            port_=ntohs(address.sin_port);
            acceptThread_=std::jthread([this](std::stop_token stop) {
                while(!stop.stop_requested()) {
                    const auto client=accept(listener_,nullptr,nullptr);
                    if(client==INVALID_SOCKET) return;
                    workers_.emplace_back([this,client](std::stop_token token){serve(client,token);});
                }
            });
        }
        ~LocalServer() {
            acceptThread_.request_stop();shutdown(listener_,SD_BOTH);closesocket(listener_);acceptThread_.join();
            for(auto& worker:workers_) worker.request_stop();
            for(auto& worker:workers_) worker.join();
            WSACleanup();
        }
        std::wstring url(std::wstring_view path) const {return L"http://127.0.0.1:"+std::to_wstring(port_)+std::wstring(path);}
        std::atomic_int requests=0;
        static constexpr std::string_view payload="verified fixture";
    private:
        void serve(SOCKET client,std::stop_token stop) {
            const DWORD timeout=1000;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
            char buffer[4096]{};const int count=recv(client,buffer,sizeof(buffer)-1,0);
            const std::string request=count>0 ? std::string(buffer,count) : std::string{};
            ++requests;
            const auto sendText=[&](std::string_view text) {
                std::size_t offset=0;
                while(offset<text.size()) {const auto n=send(client,text.data()+offset,static_cast<int>(text.size()-offset),0);if(n<=0) break;offset+=n;}
            };
            if(request.find("/headers ")!=std::string::npos) {
                while(!stop.stop_requested()) Sleep(10);
            } else if(request.find("/stall ")!=std::string::npos) {
                sendText("HTTP/1.1 200 OK\r\nContent-Length: 100000\r\nConnection: close\r\n\r\npartial");
                while(!stop.stop_requested()) Sleep(10);
            } else if(request.find("/error ")!=std::string::npos) sendText("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            else {
                const bool unknown=request.find("/unknown ")!=std::string::npos;
                const bool truncated=request.find("/truncated ")!=std::string::npos;
                sendText("HTTP/1.1 200 OK\r\n"+(unknown ? std::string{} : "Content-Length: "+std::to_string(payload.size()+(truncated ? 10 : 0))+"\r\n")+"Connection: close\r\n\r\n");
                sendText(payload);
            }
            shutdown(client,SD_BOTH);closesocket(client);
        }
        SOCKET listener_=INVALID_SOCKET;
        unsigned short port_=0;
        std::jthread acceptThread_;
        std::vector<std::jthread> workers_;
    };
    void metricsAndMailbox() {
        using namespace std::chrono;
        const InstallationTransferMeter::Clock::time_point start{};
        InstallationTransferMeter meter;
        meter.sample(0,start);meter.sample(1000,start+seconds(1));
        check(!meter.remaining(10000,start+seconds(1)),"ETA waits for two seconds");
        meter.sample(2000,start+seconds(2));
        check(meter.speed(start+seconds(2))==1000,"speed is transferred bytes over monotonic time");
        check(meter.remaining(10000,start+seconds(2)).value_or(0)==8,"ETA is remaining transfer only");
        check(!meter.remaining({},start+seconds(2)),"unknown total has no ETA");
        check(meter.speed(start+seconds(4))==0 && !meter.remaining(10000,start+seconds(4)),"stalled transfer clears speed and ETA");
        meter.sample(100,start+seconds(5));check(meter.speed(start+seconds(5))==0,"a new transfer resets its estimator");
        meter.reset();
        for(int i=0;i<=10;++i) meter.sample(static_cast<std::uint64_t>(i)*1000,start+seconds(i));
        check(meter.speed(start+seconds(10))==1000,"rolling window discards old traffic");

        InstallationMailbox mailbox;mailbox.begin(1);
        InstallationProgress progress;progress.activity=InstallationActivity::Downloading;
        check(mailbox.publish(1,progress,start),"first sample requests immediate notification");
        check(mailbox.take().has_value(),"mailbox yields first sample");
        progress.receivedBytes=1;
        check(!mailbox.publish(1,progress,start+milliseconds(50)),"consuming a sample must not bypass throttling");
        progress.receivedBytes=2;mailbox.publish(1,progress,start+milliseconds(100));
        check(mailbox.take()->progress.receivedBytes==2,"only the latest pending sample is retained");
        progress.state=InstallationStepState::Complete;
        check(mailbox.publish(1,progress,start+milliseconds(110)),"step completion is immediate");
        progress.stage=InstallationStage::Minecraft;progress.state=InstallationStepState::Active;
        mailbox.publish(1,progress,start+milliseconds(120));
        check(mailbox.take()->steps[0]==InstallationStepState::Complete,"coalescing preserves completed stages");
        mailbox.finish(1,{InstallationOutcome::Success});
        check(mailbox.take()->result->outcome==InstallationOutcome::Success,"terminal result cannot be lost");
        check(!mailbox.publish(1,progress,start+seconds(1)),"progress cannot overwrite terminal result");
        mailbox.begin(2);check(!mailbox.publish(1,progress,start+seconds(2)),"old attempts cannot update mailbox");

        InstallationTrackingState state;state.begin();
        check(!state.apply(0,progress,start),"old attempt is ignored by UI state");
        state.cancelling=true;state.finish(state.attempt,{InstallationOutcome::Success});
        check(state.result->outcome==InstallationOutcome::Cancelled,"cancellation wins simultaneous success");
        state.begin();check(!state.finish(state.attempt-1,{InstallationOutcome::Success}),"late terminal message cannot finish new attempt");
    }
    void httpAndDownloads(const std::filesystem::path& directory) {
        LocalServer server;HttpDownloadClient http;
        check(http.getText(server.url(L"/ok")).content==LocalServer::payload,"metadata request returns body");
        const auto bad=http.getText(server.url(L"/error"));
        check(!bad.success && bad.statusCode==503 && bad.failure==InstallationFailure::Http,"HTTP errors remain typed");
        const auto path=directory/L"http.bin";
        const auto unknown=http.downloadFile(server.url(L"/unknown"),path,{},{});
        check(unknown.success && read(path)==LocalServer::payload,"unknown length download succeeds");
        const auto truncated=http.downloadFile(server.url(L"/truncated"),path,{},{});
        check(!truncated.success && !std::filesystem::exists(path),"interrupted download removes partial file");
        for(const auto route:{L"/headers",L"/stall"}) {
            std::stop_source source;
            std::jthread cancel([&] {Sleep(100);source.request_stop();});
            const auto started=std::chrono::steady_clock::now();
            const auto result=route==std::wstring_view(L"/headers") ? http.getText(server.url(route),source.get_token()) :
                http.downloadFile(server.url(route),path,source.get_token(),{});
            check(result.cancelled && !result.success,"metadata and stalled body are cancellable");
            check(std::chrono::steady_clock::now()-started<std::chrono::seconds(2),"cancellation does not wait for receive timeout");
            cancel.join();
        }
        write(directory/L"expected.bin",LocalServer::payload);
        const auto hash=FileHash::sha256(directory/L"expected.bin");
        write(directory/L"cached.bin",LocalServer::payload);
        std::vector<VerifiedDownload> files={
            {server.url(L"/ok"),directory/L"cached.bin",hash,LocalServer::payload.size(),FileHashAlgorithm::Sha256},
            {server.url(L"/ok"),directory/L"new1.bin",hash,LocalServer::payload.size(),FileHashAlgorithm::Sha256},
            {server.url(L"/ok"),directory/L"new2.bin",hash,LocalServer::payload.size(),FileHashAlgorithm::Sha256}};
        std::vector<InstallationProgress> samples;
        const int before=server.requests;
        const auto result=VerifiedDownloader{}.downloadAll(files,{},[&](const auto& value){samples.push_back(value);},2);
        check(result.success && server.requests-before==2,"valid cache is not downloaded");
        const auto& last=samples.back();
        check(last.receivedBytes==LocalServer::payload.size()*2 && last.totalBytes==LocalServer::payload.size()*2,
            "parallel traffic and denominator exclude cache");
        check(last.reusedFiles==1 && last.completedFiles==2 && last.totalFiles==2,"verified and reused counts are separate");
        bool premature=false;for(const auto& value:samples) if(value.percent==100 && value.completedFiles!=value.totalFiles) premature=true;
        check(!premature,"100 percent requires hash verification and publication");
        const int cachedBefore=server.requests;
        check(VerifiedDownloader{}.downloadAll(files,{},{}).allAvailable && server.requests==cachedBefore,"second attempt reuses all verified files");
        files={{server.url(L"/unknown"),directory/L"unknown.bin",hash,0,FileHashAlgorithm::Sha256}};
        samples.clear();
        check(VerifiedDownloader{}.downloadAll(files,{},[&](const auto& value){samples.push_back(value);}).success,"unknown manifest size can be verified by hash");
        check(!samples.back().totalBytes && !samples.back().percent,"unknown size never invents a denominator");
        write(directory/L"corrupt.bin","old content");
        files={{server.url(L"/ok"),directory/L"corrupt.bin",std::string(64,'0'),LocalServer::payload.size(),FileHashAlgorithm::Sha256}};
        const auto corrupt=VerifiedDownloader{}.downloadAll(files,{},{});
        check(corrupt.failure==InstallationFailure::Integrity && read(directory/L"corrupt.bin")=="old content",
            "bad hashes preserve the existing destination");
        check(!std::filesystem::exists(directory/L"corrupt.bin.part"),"failed verification cleans temporary file");
        std::stop_source source;
        files={{server.url(L"/ok"),directory/L"keep.bin",hash,LocalServer::payload.size(),FileHashAlgorithm::Sha256},
            {server.url(L"/stall"),directory/L"cancel.bin",hash,100000,FileHashAlgorithm::Sha256}};
        const auto cancelled=VerifiedDownloader{}.downloadAll(files,source.get_token(),[&](const auto& value) {
            if(value.completedFiles==1 && value.receivedBytes>LocalServer::payload.size()) source.request_stop();
        },1);
        check(source.stop_requested() && !cancelled.success && read(directory/L"keep.bin")==LocalServer::payload && !std::filesystem::exists(directory/L"cancel.bin.part"),
            "cancel preserves published files and cleans incomplete download");
        const auto disk=http.downloadFile(server.url(L"/ok"),directory/L"keep.bin"/L"blocked",{},{});
        check(!disk.success && disk.failure==InstallationFailure::Disk,"filesystem failures return disk category");
    }
    struct FakeFiles : IFileServerService {
        int calls=0;bool fail=false;
        FileServerManifestResponse fetchManifest(std::stop_token={}) override {
            ++calls;if(fail) return {false,503,{},L"HTTP 503 https://example.invalid/?access_token=secret",0,InstallationFailure::Http};
            return {true,200,L"{}"};
        }
    };
    struct FakeJava : IJavaRuntimeService {
        int calls=0;
        bool isInstalled() const override {return false;}
        std::filesystem::path runtimeDirectory() const override {return {};}
        JavaRuntimeResult ensureInstalled(std::stop_token,const JavaRuntimeProgressCallback& progress) override {
            ++calls;progress({InstallationStage::Java,InstallationActivity::Extracting,InstallationStepState::Active,L"Extraindo Java"});
            progress({InstallationStage::Java,InstallationActivity::Extracting,InstallationStepState::Complete,L"Java pronto"});
            return {true,true,L"Pronto"};
        }
    };
    struct FakeLauncher : IMinecraftLauncherService {
        bool ready=false,hasJava=false;int launches=0;
        MinecraftLaunchResult validateInstallation() const override {
            return ready ? MinecraftLaunchResult{true,L"Pronto"} : MinecraftLaunchResult{false,L"Pendente",0,
                hasJava ? MinecraftLaunchIssue::InstallationIncomplete : MinecraftLaunchIssue::JavaMissing};
        }
        MinecraftLaunchResult launch(std::wstring_view) override {++launches;return {true,L"Iniciado"};}
        MinecraftLaunchResult launch(const MinecraftIdentity&) override {++launches;return {true,L"Iniciado"};}
        bool isGameRunning() const override {return false;}
        std::filesystem::path gameDirectory() const override {return {};}
    };
    struct FakeGame : IGameInstallationService {
        FakeLauncher& launcher;int calls=0;
        explicit FakeGame(FakeLauncher& value):launcher(value) {}
        GameInstallationResult ensureInstalled(std::wstring_view,std::stop_token stop,const GameInstallationProgressCallback& progress) override {
            ++calls;
            for(const auto stage:{InstallationStage::Minecraft,InstallationStage::Natives,InstallationStage::Forge,InstallationStage::Mods}) {
                progress({stage,stage==InstallationStage::Natives ? InstallationActivity::Extracting : InstallationActivity::Installing,
                    InstallationStepState::Active,L"Preparando"});
                if(stop.stop_requested()) return {false,L"Cancelada",0};
                progress({stage,InstallationActivity::Verifying,InstallationStepState::Complete,L"Pronto"});
            }
            launcher.ready=true;return {true,L"Pronto",5};
        }
    };
    void coordinator(const std::filesystem::path& directory) {
        LauncherSettings settings;settings.gameDirectory=directory;
        FakeFiles files;FakeJava java;FakeLauncher launcher;FakeGame game(launcher);
        InstallationCoordinator coordinator(files,java,game,launcher,settings);
        std::vector<InstallationProgress> samples;
        const auto first=coordinator.run({},[&](const auto& value){samples.push_back(value);});
        check(first.outcome==InstallationOutcome::Success && java.calls==1 && game.calls==1,"first installation prepares Java and game");
        check(first.synchronizedFiles==5 && launcher.launches==0,"coordinator validates but UI owns automatic launch");
        check(samples.back().stage==InstallationStage::Validation && samples.back().state==InstallationStepState::Complete,"final stage completes after validation");
        write(directory/L"worlds"/L"saved-world"/L"level.dat","preserved world");
        check(coordinator.run({},{}).outcome==InstallationOutcome::Success && files.calls==2 && java.calls==1,"retry re-fetches manifest and reuses Java");
        const auto customJava=directory.parent_path()/L"custom-java";
        write(customJava/L"bin"/L"java.exe","test-only executable");
        write(customJava/L"release","JAVA_VERSION=\"17.0.2\"\n");
        settings.javaExecutable=customJava/L"bin"/L"java.exe";
        check(coordinator.run({},{}).outcome==InstallationOutcome::Success && java.calls==1,"validated custom Java skips portable preparation");
        settings.javaExecutable.clear();
        files.fail=true;
        check(coordinator.run({},{}).failure==InstallationFailure::Http,"typed failure reaches coordinator");
        const auto log=read(directory/L"logs"/L"installation.log");
        check(log.find("secret")==std::string::npos && log.find("access_token")==std::string::npos,"installation log excludes credential-bearing URL");
        files.fail=false;launcher.ready=false;launcher.hasJava=true;
        for(const auto stage:{InstallationStage::Natives,InstallationStage::Forge,InstallationStage::Validation}) {
            std::stop_source stop;
            const auto result=coordinator.run(stop.get_token(),[&](const auto& value) {if(value.stage==stage && value.state==InstallationStepState::Active) stop.request_stop();});
            check(result.outcome==InstallationOutcome::Cancelled && launcher.launches==0,"cancel in extraction, Forge or validation cannot launch");
            launcher.ready=false;
        }
        std::stop_source stopped;stopped.request_stop();
        check(GameInstallationService(settings).ensureInstalled(L"{}",stopped.get_token(),{}).outcome==InstallationOutcome::Cancelled,
            "game service exposes cancellation distinctly");
        check(JavaRuntimeService{}.ensureInstalled(stopped.get_token(),{}).outcome==InstallationOutcome::Cancelled,
            "Java service exposes cancellation distinctly");
        check(read(directory/L"worlds"/L"saved-world"/L"level.dat")=="preserved world","cancel and retry preserve existing worlds");
    }
    void subprocessCancellation() {
        wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
        std::wstring command=L"\""+std::wstring(executable)+L"\" --waiting-child";
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        check(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE,
            "test-owned installation subprocess starts");
        if(!process.hProcess) return;
        CloseHandle(process.hThread);
        std::stop_source stop;std::jthread cancel([&] {Sleep(100);stop.request_stop();});
        const auto started=std::chrono::steady_clock::now();
        const auto result=waitForInstallationProcess(process.hProcess,stop.get_token());
        check(result.cancelled && WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0,"tar/Forge wait helper terminates cancelled subprocess");
        check(std::chrono::steady_clock::now()-started<std::chrono::seconds(2),"subprocess cancellation has bounded latency");
        CloseHandle(process.hProcess);cancel.join();
    }
}
int main(int argc,char** argv) {
    if(argc==2 && std::string_view(argv[1])=="--waiting-child") {Sleep(60000);return 0;}
    const auto root=std::filesystem::temp_directory_path()/L"EternalInstallationTests"/std::to_wstring(GetCurrentProcessId());
    std::filesystem::create_directories(root);
    try {metricsAndMailbox();httpAndDownloads(root/L"downloads");coordinator(root/L"instance");subprocessCancellation();}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';++failures;}
    // Delete only the isolated test-owned directory; never touch a real game instance.
    if(root.is_absolute() && root.parent_path()==std::filesystem::temp_directory_path()/L"EternalInstallationTests") std::filesystem::remove_all(root);
    std::cout<<"Installation tracking failures: "<<failures<<'\n';
    return failures ? 1 : 0;
}
