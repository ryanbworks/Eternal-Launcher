#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

enum class InstallationStage { Java, Minecraft, Natives, Forge, Mods, Validation };
enum class InstallationActivity { Checking, Downloading, Verifying, Extracting, Installing, Publishing };
enum class InstallationStepState { Waiting, Active, Complete, Available, Failed, Cancelled };
enum class InstallationOutcome { Success, Cancelled, Failed };
enum class InstallationFailure { None, Network, Http, Disk, Integrity, Process, Configuration };

struct InstallationProgress {
    InstallationStage stage = InstallationStage::Java;
    InstallationActivity activity = InstallationActivity::Checking;
    InstallationStepState state = InstallationStepState::Active;
    std::wstring message;
    std::optional<int> percent;
    std::uint64_t receivedBytes = 0;
    std::optional<std::uint64_t> totalBytes;
    std::size_t completedFiles = 0;
    std::size_t totalFiles = 0;
    std::size_t reusedFiles = 0;
    // Distinguishes successive transfers within the same stage (e.g. Forge repair).
    std::uint64_t transferId = 0;
};
using InstallationProgressCallback = std::function<void(const InstallationProgress&)>;

struct InstallationResult {
    InstallationOutcome outcome = InstallationOutcome::Failed;
    InstallationFailure failure = InstallationFailure::None;
    std::wstring message;
    std::wstring technicalDetail;
    std::size_t synchronizedFiles = 0;
};

inline const wchar_t* installationStageLabel(InstallationStage stage) {
    constexpr const wchar_t* labels[] = {L"Verificações e Java 17", L"Minecraft e assets",
        L"Bibliotecas nativas", L"Forge", L"Mods e configurações", L"Validação final"};
    return labels[static_cast<std::size_t>(stage)];
}
inline const wchar_t* installationStepLabel(InstallationStepState state) {
    constexpr const wchar_t* labels[] = {L"Aguardando",L"Em andamento",L"Concluída",
        L"Já disponível",L"Falhou",L"Cancelada"};
    return labels[static_cast<std::size_t>(state)];
}

// Pure, clock-injected transfer estimator. Cache hits never enter receivedBytes.
class InstallationTransferMeter {
public:
    using Clock = std::chrono::steady_clock;
    void reset() { samples_.clear(); started_.reset(); lastTraffic_.reset(); bytes_ = 0; }
    void sample(std::uint64_t bytes, Clock::time_point now) {
        if (!started_ || bytes < bytes_) { reset(); started_ = now; }
        if (bytes > bytes_) lastTraffic_ = now;
        bytes_ = bytes;
        samples_.push_back({now,bytes});
        while (samples_.size()>2 && samples_[1].first <= now-std::chrono::seconds(5)) samples_.pop_front();
    }
    double speed(Clock::time_point now) const {
        if (samples_.size()<2 || !lastTraffic_ || now-*lastTraffic_>=std::chrono::seconds(2)) return 0;
        const double seconds=std::chrono::duration<double>(now-samples_.front().first).count();
        return seconds>0 ? static_cast<double>(bytes_-samples_.front().second)/seconds : 0;
    }
    std::optional<double> remaining(std::optional<std::uint64_t> total, Clock::time_point now) const {
        const double rate=speed(now);
        if (!total || !started_ || now-*started_<std::chrono::seconds(2) || rate<=0 || bytes_>=*total) return {};
        return static_cast<double>(*total-bytes_)/rate;
    }
private:
    std::deque<std::pair<Clock::time_point,std::uint64_t>> samples_;
    std::optional<Clock::time_point> started_,lastTraffic_;
    std::uint64_t bytes_=0;
};

struct InstallationTrackingState {
    bool visible=false;
    bool running=false;
    bool cancelling=false;
    std::uint64_t attempt=0;
    InstallationProgress progress;
    std::array<InstallationStepState,6> steps{};
    InstallationTransferMeter meter;
    std::optional<InstallationResult> result;
    void begin() {
        ++attempt; visible=true; running=true; cancelling=false; steps.fill(InstallationStepState::Waiting);
        progress={}; steps[0]=InstallationStepState::Active; meter.reset(); result.reset();
    }
    bool apply(std::uint64_t generation,const InstallationProgress& value,InstallationTransferMeter::Clock::time_point now) {
        if (generation!=attempt || !running || cancelling) return false;
        if (value.stage!=progress.stage || value.activity!=progress.activity || value.transferId!=progress.transferId) meter.reset();
        progress=value; steps[static_cast<std::size_t>(value.stage)]=value.state;
        if (value.activity==InstallationActivity::Downloading) meter.sample(value.receivedBytes,now);
        return true;
    }
    bool finish(std::uint64_t generation,InstallationResult value) {
        if (generation!=attempt || !running) return false;
        if (cancelling) { value.outcome=InstallationOutcome::Cancelled; value.failure=InstallationFailure::None; }
        running=false; cancelling=false;
        if (value.outcome!=InstallationOutcome::Success) {
            steps[static_cast<std::size_t>(progress.stage)]=value.outcome==InstallationOutcome::Cancelled
                ? InstallationStepState::Cancelled : InstallationStepState::Failed;
        }
        result=std::move(value); return true;
    }
};

// One latest sample plus six durable stage states. No owning payloads in the HWND queue.
class InstallationMailbox {
public:
    struct Snapshot {
        std::uint64_t attempt=0;
        InstallationProgress progress;
        std::array<InstallationStepState,6> steps{};
        std::optional<InstallationResult> result;
    };
    void begin(std::uint64_t attempt) {
        std::scoped_lock lock(mutex_); snapshot_={}; snapshot_.attempt=attempt; dirty_=false; hasSample_=false; lastPost_={};
    }
    bool publish(std::uint64_t attempt,const InstallationProgress& progress,InstallationTransferMeter::Clock::time_point now) {
        std::scoped_lock lock(mutex_);
        if (attempt!=snapshot_.attempt || snapshot_.result) return false;
        const bool change=!hasSample_ || progress.stage!=snapshot_.progress.stage || progress.activity!=snapshot_.progress.activity || progress.state!=snapshot_.progress.state;
        snapshot_.progress=progress; snapshot_.steps[static_cast<std::size_t>(progress.stage)]=progress.state; dirty_=true;hasSample_=true;
        if (change || now-lastPost_>=std::chrono::milliseconds(200)) {lastPost_=now;return true;} return false;
    }
    void finish(std::uint64_t attempt,InstallationResult result) {
        std::scoped_lock lock(mutex_); if(attempt==snapshot_.attempt) { snapshot_.result=std::move(result); dirty_=true; }
    }
    std::optional<Snapshot> take() {
        std::scoped_lock lock(mutex_); if(!dirty_) return {}; dirty_=false; return snapshot_;
    }
private:
    std::mutex mutex_;
    Snapshot snapshot_;
    bool dirty_=false;
    bool hasSample_=false;
    InstallationTransferMeter::Clock::time_point lastPost_{};
};
