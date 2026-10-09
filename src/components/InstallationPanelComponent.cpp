#include "components/InstallationPanelComponent.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
    std::wstring amount(double bytes) {
        const auto unit=bytes>=1024*1024*1024 ? L"GB" : bytes>=1024*1024 ? L"MB" : L"KB";
        const double divider=bytes>=1024*1024*1024 ? 1024.*1024*1024 : bytes>=1024*1024 ? 1024.*1024 : 1024.;
        wchar_t value[48]{};swprintf_s(value,L"%.1f",bytes/divider);
        std::wstring result=value;std::replace(result.begin(),result.end(),L'.',L',');
        return result+L" "+unit;
    }
    Gdiplus::Color tone(InstallationStepState state) {
        switch(state) {
        case InstallationStepState::Complete:case InstallationStepState::Available:return Theme::Success;
        case InstallationStepState::Failed:return Theme::Error;
        case InstallationStepState::Cancelled:return Theme::Warning;
        case InstallationStepState::Active:return Theme::AccentLight;
        default:return Theme::TextMuted;
        }
    }
}
void InstallationPanelComponent::draw(Gdiplus::Graphics& g,const InstallationTrackingState& state,float phase,bool cancelFocus,bool logFocus,
    float cancelHover,float logHover,bool cancelPressed,bool logPressed,float fill,std::array<float,6> completion) const {
    DrawingUtils::fillRoundedRectangle(g,{36,112,744,592},Theme::PanelRadius,Theme::Surface);
    const auto& progress=state.progress;
    const bool failed=state.result && state.result->outcome==InstallationOutcome::Failed;
    const bool cancelled=state.result && state.result->outcome==InstallationOutcome::Cancelled;
    const auto title=state.cancelling ? L"Cancelando instalação..." : failed ? L"Preparação interrompida" : cancelled ? L"Instalação cancelada" : L"Preparando seu jogo";
    DrawingUtils::drawText(g,title,{60,136,680,36},26,Gdiplus::FontStyleBold,Theme::TextPrimary);
    DrawingUtils::drawText(g,L"Arquivos completos são verificados e reaproveitados a cada tentativa.",
        {60,181,680,26},13,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    for(std::size_t i=0;i<state.steps.size();++i) {
        const float y=221+static_cast<float>(i)*36;
        const auto step=state.steps[i];
        if(step==InstallationStepState::Active || step==InstallationStepState::Failed || step==InstallationStepState::Cancelled)
            DrawingUtils::fillRoundedRectangle(g,{60,y,696,32},8,Theme::SurfaceElevated);
        if(step==InstallationStepState::Complete || step==InstallationStepState::Available) {
            Gdiplus::Pen check(tone(step),1.8F);
            g.DrawLine(&check,73.0F,y+16,77.0F,y+20);g.DrawLine(&check,77.0F,y+20,84.0F,y+12);
            if(completion[i]>0) {
                const float radius=6+(1-completion[i])*8;
                Gdiplus::Pen confirmation(Gdiplus::Color(static_cast<BYTE>(150*completion[i]),55,214,144),1.5F);
                g.DrawEllipse(&confirmation,78-radius,y+16-radius,2*radius,2*radius);
            }
        } else {
            Gdiplus::SolidBrush dot(tone(step));g.FillEllipse(&dot,74.0F,y+12,8.0F,8.0F);
            if(step==InstallationStepState::Active && state.running && !state.cancelling) {
                Gdiplus::Pen orbit(Theme::AccentLight,1.6F);
                g.DrawArc(&orbit,70.0F,y+8,16.0F,16.0F,phase*360,115.0F);
            }
        }
        DrawingUtils::drawText(g,installationStageLabel(static_cast<InstallationStage>(i)),{96,y,428,32},14,
            step==InstallationStepState::Active ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular,Theme::TextPrimary,
            Gdiplus::StringAlignmentNear,Gdiplus::StringAlignmentCenter);
        DrawingUtils::drawText(g,installationStepLabel(step),{540,y,198,32},12,Gdiplus::FontStyleRegular,tone(step),
            Gdiplus::StringAlignmentFar,Gdiplus::StringAlignmentCenter);
    }
    Gdiplus::Pen rule(Theme::Border);g.DrawLine(&rule,60,448,756,448);
    const std::wstring message=state.cancelling ? L"Aguardando as tarefas encerrarem com segurança..." : state.result ? state.result->message : progress.message;
    DrawingUtils::drawText(g,message,{60,463,696,46},14,Gdiplus::FontStyleRegular,failed ? Theme::Error : Theme::TextPrimary);
    const bool transfer=state.running && !state.cancelling && progress.activity==InstallationActivity::Downloading;
    if(failed && progress.stage==InstallationStage::Forge) DrawingUtils::drawText(g,
        L"Detalhes adicionais em logs/forge-installer.log.",{60,520,696,26},13,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    if(transfer) {
        std::wstring bytes=amount(static_cast<double>(progress.receivedBytes));
        if(progress.totalBytes) bytes+=L" de "+amount(static_cast<double>(*progress.totalBytes));
        DrawingUtils::drawText(g,bytes,{60,514,570,26},18,Gdiplus::FontStyleBold,Theme::TextPrimary);
        if(progress.percent) DrawingUtils::drawText(g,std::to_wstring(*progress.percent)+L"%",{660,514,96,26},18,
            Gdiplus::FontStyleBold,Theme::AccentLight,Gdiplus::StringAlignmentFar);
    }
    if(state.running && !state.cancelling) {
        const Gdiplus::RectF track(60,550,696,4);
        DrawingUtils::fillRoundedRectangle(g,track,2,Theme::SurfaceHover);
        if(transfer && progress.percent) DrawingUtils::fillRoundedRectangle(g,{track.X,track.Y,
            track.Width*std::clamp(fill<0 ? static_cast<float>(*progress.percent) : fill,0.0F,100.0F)/100,4},2,Theme::Accent);
        else {
            const auto saved=g.Save();g.SetClip(track);
            const float x=track.X+(std::fmod(phase,1.0F)*1.35F-0.35F)*track.Width;
            DrawingUtils::fillRoundedRectangle(g,{x,track.Y,track.Width*.35F,4},2,Theme::Accent);g.Restore(saved);
        }
    }
    if(transfer) {
        const auto now=InstallationTransferMeter::Clock::now();
        const double speed=state.meter.speed(now);
        std::wstring details=speed>0 ? amount(speed)+L"/s" : L"Aguardando transferência...";
        if(const auto remaining=state.meter.remaining(progress.totalBytes,now)) {
            const auto seconds=static_cast<std::uint64_t>(std::ceil(*remaining));
            details+=L"  ·  Cerca de "+(seconds>=60 ? std::to_wstring((seconds+59)/60)+L" min" : std::to_wstring(seconds)+L" s")+L" para esta transferência";
        }
        DrawingUtils::drawText(g,details,{60,567,696,24},13,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    }
    if(state.running && progress.totalFiles) DrawingUtils::drawText(g,
        std::to_wstring(progress.completedFiles)+L" de "+std::to_wstring(progress.totalFiles)+L" arquivos verificados  ·  "+
        std::to_wstring(progress.reusedFiles)+L" reaproveitados",{60,598,696,24},12,Gdiplus::FontStyleRegular,Theme::TextSecondary);
    const auto button=[&](const Gdiplus::RectF& bounds,const wchar_t* label,bool enabled,bool focus,float hover,bool pressed) {
        DrawingUtils::fillRoundedRectangle(g,bounds,Theme::ControlRadius,!enabled || pressed ? Theme::SurfaceHover :
            DrawingUtils::blend(Theme::SurfaceElevated,Theme::SurfaceHover,hover));
        DrawingUtils::drawText(g,label,bounds,13,Gdiplus::FontStyleBold,enabled ? Theme::TextPrimary : Theme::TextMuted,
            Gdiplus::StringAlignmentCenter,Gdiplus::StringAlignmentCenter);
        if(focus) DrawingUtils::drawRoundedRectangle(g,{bounds.X+2,bounds.Y+2,bounds.Width-4,bounds.Height-4},8,Theme::AccentLight,2);
    };
    button(logBounds(),L"Abrir log",true,logFocus,logHover,logPressed);
    if(state.running) button(cancelBounds(),state.cancelling ? L"Cancelando..." : L"Cancelar instalação",!state.cancelling,cancelFocus,cancelHover,cancelPressed);
}
