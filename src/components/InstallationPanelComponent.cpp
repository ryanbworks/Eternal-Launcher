#include "components/InstallationPanelComponent.h"

#include "components/Layout.h"
#include "constants/LauncherConstants.h"
#include "styles/Theme.h"
#include "utils/DrawingUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace DrawingUtils;

namespace
{
    std::wstring amount(double bytes)
    {
        const auto unit = bytes >= 1024 * 1024 * 1024 ? L"GB" : bytes >= 1024 * 1024 ? L"MB" : L"KB";
        const double divider = bytes >= 1024 * 1024 * 1024 ? 1024. * 1024 * 1024 : bytes >= 1024 * 1024 ? 1024. * 1024 : 1024.;
        wchar_t value[48]{};
        swprintf_s(value, L"%.1f", bytes / divider);
        std::wstring result = value;
        std::replace(result.begin(), result.end(), L'.', L',');
        return result + L" " + unit;
    }

    bool done(InstallationStepState state)
    {
        return state == InstallationStepState::Complete || state == InstallationStepState::Available;
    }

    void chip(Gdiplus::Graphics& g, float& x, float y, const std::wstring& text)
    {
        const float width = measureText(g, text, 12, Gdiplus::FontStyleRegular) + 20;
        fillPill(g, {x, y, width, 24}, Theme::Chip);
        drawText(g, text, {x, y, width, 24}, 12, Gdiplus::FontStyleRegular, Gdiplus::Color(255, 217, 218, 228),
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
        x += width + 8;
    }

    // A comet that keeps moving along a bar whose length is unknown.
    void indeterminate(Gdiplus::Graphics& g, const Gdiplus::RectF& bar, float phase)
    {
        fillPill(g, bar, Theme::SurfaceHover);
        const auto saved = g.Save();
        g.SetClip(bar, Gdiplus::CombineModeIntersect);
        const float x = bar.X + (std::fmod(phase, 1.0F) * 1.35F - .35F) * bar.Width;
        fillPill(g, {x, bar.Y, bar.Width * .35F, bar.Height}, Theme::Accent);
        g.Restore(saved);
    }

    // Step marker: check when done, cross when stopped, orbiting ring while active.
    void marker(Gdiplus::Graphics& g, InstallationStepState step, float cx, float cy, float phase, bool moving, float completion)
    {
        if (done(step)) {
            Gdiplus::SolidBrush badge(Theme::SuccessTint);
            g.FillEllipse(&badge, cx - 11, cy - 11, 22.0F, 22.0F);
            Icons::draw(g, Icons::Icon::Check, {cx - 6, cy - 6, 12, 12}, Theme::Success, 1.8F);
            if (completion > 0) {
                const float radius = 11 + (1 - completion) * 8;
                Gdiplus::Pen pulse(Gdiplus::Color(static_cast<BYTE>(150 * completion), 55, 214, 144), 1.5F);
                g.DrawEllipse(&pulse, cx - radius, cy - radius, 2 * radius, 2 * radius);
            }
        } else if (step == InstallationStepState::Failed || step == InstallationStepState::Cancelled) {
            const auto color = step == InstallationStepState::Failed ? Theme::Error : Theme::Warning;
            Gdiplus::SolidBrush back(Theme::Surface);
            g.FillEllipse(&back, cx - 11, cy - 11, 22.0F, 22.0F);
            Gdiplus::Pen ring(color, 2);
            g.DrawEllipse(&ring, cx - 10, cy - 10, 20.0F, 20.0F);
            Icons::draw(g, Icons::Icon::Close, {cx - 5, cy - 5, 10, 10}, color, 1.8F);
        } else if (step == InstallationStepState::Active) {
            Gdiplus::SolidBrush back(Theme::Surface);
            g.FillEllipse(&back, cx - 11, cy - 11, 22.0F, 22.0F);
            Gdiplus::Pen ring(Theme::Accent, 2);
            g.DrawEllipse(&ring, cx - 10, cy - 10, 20.0F, 20.0F);
            Gdiplus::SolidBrush center(Theme::Accent);
            g.FillEllipse(&center, cx - 4, cy - 4, 8.0F, 8.0F);
            if (moving) {
                Gdiplus::Pen orbit(Theme::AccentLight, 2);
                g.DrawArc(&orbit, cx - 10, cy - 10, 20.0F, 20.0F, phase * 360, 100.0F);
            }
        } else {
            Gdiplus::SolidBrush back(Theme::Surface);
            g.FillEllipse(&back, cx - 11, cy - 11, 22.0F, 22.0F);
            Gdiplus::Pen ring(Theme::Border, 2);
            g.DrawEllipse(&ring, cx - 10, cy - 10, 20.0F, 20.0F);
        }
    }
}

float InstallationPanelComponent::overallPercent(const InstallationTrackingState& state, float fill)
{
    float completed = 0;
    for (const auto step : state.steps) if (done(step)) completed += 1;
    const auto& progress = state.progress;
    const auto active = state.steps[static_cast<std::size_t>(progress.stage)];
    if (state.running && active == InstallationStepState::Active && progress.activity == InstallationActivity::Downloading && progress.percent)
        completed += std::clamp(fill >= 0 ? fill : static_cast<float>(*progress.percent), 0.0F, 100.0F) / 100;
    return std::clamp(completed / 6 * 100, 0.0F, 100.0F);
}

void InstallationPanelComponent::draw(Gdiplus::Graphics& g, const InstallationTrackingState& state, float phase,
                                      const ControlVisuals& visuals, float fill, std::array<float, 6> completion) const
{
    // Compact card in the hero's slot: header, overall bar, a horizontal stepper,
    // details for the current step and the two actions. The rest of Home stays visible.
    const auto& panel = Layout::Installation;
    fillRoundedRectangle(g, panel, Theme::PanelRadius, Theme::Surface);
    drawRoundedRectangle(g, {panel.X + .5F, panel.Y + .5F, panel.Width - 1, panel.Height - 1}, Theme::PanelRadius, Theme::Divider);
    const auto& progress = state.progress;
    const bool failed = state.result && state.result->outcome == InstallationOutcome::Failed;
    const bool cancelled = state.result && state.result->outcome == InstallationOutcome::Cancelled;
    const bool moving = state.running && !state.cancelling;
    const float left = panel.X + 30, width = panel.Width - 60;

    const auto title = state.cancelling ? L"Cancelando instalação..." : failed ? L"Preparação interrompida" :
        cancelled ? L"Instalação cancelada" : L"Preparando seu mundo";
    drawDisplayText(g, title, {left, panel.Y + 22, width - 140, 34}, 23, Theme::TextPrimary);
    const int stepNumber = static_cast<int>(progress.stage) + 1;
    const std::wstring subtitle = moving
        ? L"Etapa " + std::to_wstring(stepNumber) + L" de 6 · " + installationStageLabel(progress.stage) + L". Pode cancelar: o que já foi verificado é reaproveitado."
        : L"Arquivos completos são verificados e reaproveitados a cada tentativa.";
    drawText(g, subtitle, {left, panel.Y + 58, width - 140, 20}, 12.5F, Gdiplus::FontStyleRegular, Theme::TextSecondary);
    const float overall = overallPercent(state, fill);
    drawDisplayText(g, std::to_wstring(static_cast<int>(std::floor(overall))) + L"%", {left + width - 140, panel.Y + 20, 140, 48}, 34,
        failed ? Theme::Error : Theme::AccentLight, Gdiplus::StringAlignmentFar);

    const Gdiplus::RectF track{left, panel.Y + 92, width, 6};
    fillPill(g, track, Theme::SurfaceHover);
    if (overall > 0) {
        const Gdiplus::RectF filled{track.X, track.Y, std::max(6.0F, track.Width * overall / 100), track.Height};
        fillPill(g, filled, failed ? Theme::Error : Theme::Accent);
        if (moving) {
            // A highlight travels along the filled part while work is in progress.
            const auto saved = g.Save();
            auto clip = createRoundedPath(filled, track.Height / 2);
            g.SetClip(clip.get(), Gdiplus::CombineModeIntersect);
            const float center = filled.X - 40 + (filled.Width + 80) * std::fmod(phase, 1.0F);
            Gdiplus::LinearGradientBrush shine(Gdiplus::PointF(center - 40, 0), Gdiplus::PointF(center + 40, 0),
                Gdiplus::Color(0, 255, 255, 255), Gdiplus::Color(0, 255, 255, 255));
            const Gdiplus::Color colors[] = {Gdiplus::Color(0, 255, 255, 255), Gdiplus::Color(120, 255, 255, 255), Gdiplus::Color(0, 255, 255, 255)};
            const Gdiplus::REAL positions[] = {0, .5F, 1};
            shine.SetInterpolationColors(colors, positions, 3);
            g.FillRectangle(&shine, center - 40, filled.Y, 80.0F, filled.Height);
            g.Restore(saved);
        }
    }

    // Horizontal stepper: six markers joined by a line that fills as steps finish.
    const auto& stepper = Layout::InstallationStepper;
    const float column = stepper.Width / 6;
    const float cy = stepper.Y + 11;
    for (std::size_t i = 0; i + 1 < state.steps.size(); ++i) {
        const float x0 = stepper.X + column * (static_cast<float>(i) + .5F) + 14;
        const float x1 = stepper.X + column * (static_cast<float>(i) + 1.5F) - 14;
        Gdiplus::Pen link(done(state.steps[i]) ? Theme::Success : Theme::Border, 2);
        g.DrawLine(&link, x0, cy, x1, cy);
    }
    for (std::size_t i = 0; i < state.steps.size(); ++i) {
        const auto step = state.steps[i];
        const float cx = stepper.X + column * (static_cast<float>(i) + .5F);
        const bool current = i == static_cast<std::size_t>(progress.stage) && !done(step) && step != InstallationStepState::Waiting;
        marker(g, step, cx, cy, phase, moving, completion[i]);
        const auto labelColor = current ? Theme::TextPrimary : done(step) ? Theme::TextSecondary : Theme::TextMuted;
        drawText(g, installationStageLabel(static_cast<InstallationStage>(i)), {cx - column / 2 + 4, stepper.Y + 26, column - 8, 32},
            11.5F, current ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, labelColor, Gdiplus::StringAlignmentCenter);
    }

    // Details for the step in progress, or the result once it ended.
    const auto& detail = Layout::InstallationDetail;
    fillRoundedRectangle(g, detail, 14, failed ? Gdiplus::Color(255, 43, 26, 32) : Gdiplus::Color(255, 27, 26, 43));
    drawRoundedRectangle(g, {detail.X + .5F, detail.Y + .5F, detail.Width - 1, detail.Height - 1}, 14,
        failed ? Theme::DangerOutline : Theme::AccentOutline);
    const float inner = detail.X + 18, innerWidth = detail.Width - 36;
    std::wstring message = state.cancelling ? L"Aguardando as tarefas encerrarem com segurança..." :
        state.result ? state.result->message : progress.message;
    if (failed && progress.stage == InstallationStage::Forge) message += L" Detalhes em logs/forge-installer.log.";
    const bool transfer = moving && progress.activity == InstallationActivity::Downloading;
    if (transfer) {
        std::wstring heading = message;
        if (progress.totalFiles) heading = L"Baixando · " + std::to_wstring(progress.completedFiles) + L" de " + std::to_wstring(progress.totalFiles);
        drawText(g, heading, {inner, detail.Y + 12, innerWidth * .6F, 20}, 13, Gdiplus::FontStyleBold, Theme::TextPrimary);
        std::wstring bytes = amount(static_cast<double>(progress.receivedBytes));
        if (progress.totalBytes) bytes += L" de " + amount(static_cast<double>(*progress.totalBytes));
        drawText(g, bytes, {inner + innerWidth * .4F, detail.Y + 12, innerWidth * .6F, 20}, 13, Gdiplus::FontStyleRegular,
            Theme::TextSecondary, Gdiplus::StringAlignmentFar);
        const Gdiplus::RectF bar{inner, detail.Y + 38, innerWidth, 6};
        if (progress.percent) {
            fillPill(g, bar, Theme::SurfaceHover);
            const float value = std::clamp(fill < 0 ? static_cast<float>(*progress.percent) : fill, 0.0F, 100.0F);
            if (value > 0) fillPill(g, {bar.X, bar.Y, std::max(6.0F, bar.Width * value / 100), bar.Height}, Theme::Accent);
        } else {
            indeterminate(g, bar, phase);
        }
        float chipX = inner;
        const auto now = InstallationTransferMeter::Clock::now();
        const double speed = state.meter.speed(now);
        std::wstring rate = speed > 0 ? amount(speed) + L"/s" : L"Aguardando transferência...";
        if (const auto remaining = state.meter.remaining(progress.totalBytes, now)) {
            const auto seconds = static_cast<std::uint64_t>(std::ceil(*remaining));
            rate += L" · ~" + (seconds >= 60 ? std::to_wstring((seconds + 59) / 60) + L" min" : std::to_wstring(seconds) + L" s");
        }
        chip(g, chipX, detail.Y + 56, rate);
        if (progress.totalFiles) {
            chip(g, chipX, detail.Y + 56, std::to_wstring(progress.completedFiles) + L" verificados");
            chip(g, chipX, detail.Y + 56, std::to_wstring(progress.reusedFiles) + L" reaproveitados");
        }
    } else {
        drawText(g, message, {inner, detail.Y + 12, innerWidth, moving ? 40.0F : 68.0F}, 13, Gdiplus::FontStyleRegular,
            failed ? Theme::Danger : Theme::TextSecondary);
        if (moving) indeterminate(g, {inner, detail.Y + 66, innerWidth, 6}, phase);
    }

    Buttons::draw(g, Layout::InstallationLog, L"Abrir log", visuals(LauncherConstants::InstallationLogButtonId),
        Buttons::Style::Ghost, Icons::Icon::File);
    if (state.running)
        Buttons::draw(g, Layout::InstallationCancel, state.cancelling ? L"Cancelando..." : L"Cancelar instalação",
            visuals(LauncherConstants::CancelInstallationButtonId), Buttons::Style::Danger);
}
