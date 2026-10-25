# Eternal Launcher animations

Motion preserves the dark/violet theme and the Win32 controls. No new library was
added. Drawing still uses GDI+ with the existing frame buffer.

- On startup, the landscape is revealed through the portal opening in 750 ms. Text
  and actions are available from the first frame. The animation does not replay when
  returning from settings.
- The access option's background slides between **Nickname local** and **Original**.
  The form and the accessible selection change immediately, without waiting for the
  animation.
- Settings slides in from the right in 340 ms; the main screen returns from the left
  in 280 ms. The header stays still. The cubic ease-out also moves the HWNDs of the
  fields and buttons, in one batch per frame, without recreating fonts. Focus, Tab,
  editing and actions stay available during the motion. Opening the same page again
  neither restarts the transition nor discards the draft.
- Entering and leaving installation tracking reveals the panel in 240 ms. The
  previous image is kept in a temporary buffer without overlapping text. Cancel,
  Open log and the console on the right respond immediately.
- The transfer bar advances smoothly to the real percentage. Percentage, bytes and
  counts keep showing the received sample. Each new transfer restarts the bar.
  Cancel and Open log get an interpolated hover.

The **Show animations in Windows** preference, read through
`SPI_GETCLIENTAREAANIMATION`, makes motion instant while keeping the focus, selection
and progress cues. `WM_SETTINGCHANGE` reapplies the preference while running.
The timer requests `USER_TIMER_MINIMUM` (10 ms) during transitions, indeterminate
operations and the start screen's atmosphere while in the foreground. It stops when
the window is hidden/minimized, while closing and in settings after the entrance.
The atmosphere stops requesting frames when it loses the foreground; interaction
effects already started finish normally. The reduced-motion preference disables all
of this.

On the landscape, 40 particles in three layers float at different speeds, with
different sizes, trails and glow. Another 24 follow two arcs of the portal. Hover
intensifies the energy and the initial opening produces a brief activation. The
atmosphere stays on the illustration side, never covering text. The base drift cycle
lasts eight seconds; nearer layers move faster.

The Play button gets a 480 ms soft-edged light band on hover or keyboard focus, a
light that follows the cursor with 55 ms interpolation and a glint that runs along
the border. The icon moves forward 10 logical pixels with a short trail; pressing
compresses it three pixels within the original rectangle. Click and Space produce a
380 ms ripple. The clickable area stays the same. The gear rotates 60 degrees with
the hover. Stages in progress get an orbiting marker; completed or already available
stages show a check and a 420 ms confirmation pulse. Each state's text and the metrics
stay instant.

With reduced motion, particles, portal arcs, the light band, ripples and pulses are
removed. Selection, hover color, focus, checks and the pressed response remain,
without interpolation. Installation keeps its textual indication.

The landscape and logo images are downscaled with high-quality interpolation once
per size/scale and stored in premultiplied-alpha bitmaps. Following frames reuse
those bitmaps. Size or DPI changes rebuild the cache at the displayed resolution.
Animation frames don't repeat the synchronization of control visibility, labels and
state. A high-precision monotonic clock computes progress, without the quantization
of `GetTickCount64`. The buffer is presented with explicit source and destination in
pixels, so GDI+ never applies another DPI scale. The test host uses the same
per-monitor DPI awareness as the application. A regression test compares presented
pixels with the buffer even when the bitmap's declared physical resolution changes.

The transition cache is freed when it finishes, on resize or when motion is reduced.
Interruptions start from the last rendered frame. Presentation never changes the
decisions about installation, cancellation, login or launching Minecraft.

## Verification

Release built with `/W4`. All six CTest suites passed, including AutoUpdate.
`WindowPaintingTests` checks inverted selection during motion, scene-change
interruption, cache disposal, cancellation available during a transition, the bar
restarting per transfer and the timer pausing while hidden and minimized. It also
checks immediate feedback in reduced-motion mode. The preference is injected in the
test; the Windows setting is not changed.

Reproducible preview, without calling external services:

```powershell
./out/build/installation/WindowPaintingTests.exe --capture-motion .impeccable/review/motion
./out/build/installation/EternalLauncher.exe --render-preview .impeccable/review/motion/states
```

The images use the production renderer with simulated states, at 1180 × 730 and
944 × 584. Settings is captured with `WM_PRINTCLIENT` on the parent and `WM_PRINT` on
each child, with explicit origin and clipping, including the native EDITs and
buttons in motion. The other sequences use the buffer and a synthetically drawn
"Steve" nickname, since the native EDIT is outside it. They prove composition and
motion phases, but not physical presentation on the monitor, physical mouse
interaction, screen readers or DPI changes across monitors. Real Minecraft and Forge
are not part of this validation. No update was published.

## Smoothness measurement

`WindowPaintingTests.exe --benchmark-motion` separately measures buffer drawing and
the intervals between `WM_PAINT` in a visible native window with the same controls,
per-monitor DPI and `WS_EX_COMPOSITED` as the application. The host uses simulated
states, without network: it tracks portal/indicator activity for 1.4 s and then
continuous settings transitions for another 1.4 s at each size. Drawing is measured
over 40 samples after 8 warm-up frames. The suite has no absolute-time assertions:
the measurement depends on the machine and system load.

Local results from 2026-10-04, medians before/after:

| Measure | Before | After |
|---|---:|---:|
| Start screen drawing, 1180 × 730 | 64.32 ms | 10.86 ms |
| Portal drawing, 1180 × 730 | 67.82 ms | 10.76 ms |
| Installation drawing, 1180 × 730 | 33.54 ms | 7.15 ms |
| Interval between paints, 1180 × 730 | 72.63 ms | 15.40 ms |
| Start screen drawing, 944 × 584 | 63.80 ms | 9.63 ms |
| Interval between paints, 944 × 584 | 68.48 ms | 15.20 ms |

The final test received 88/90 paints in 1.4 s at normal/compact sizes, consistent
with the target of roughly 60 frames per second. The 95th percentile of intervals
was 19.04/21.16 ms. This measures the test window's execution, without guaranteeing
the compositor's physical cadence on every computer. The first cache build and
resizing are excluded from the warm-up medians.

The demo GIF uses 20 ms samples (50 FPS during motion), with pauses on complete
screens. It is not a recording of the benchmark. The previous version used few
samples for selection and panel changes, which also made the preview look choppy.

The timer is not an FPS guarantee: [the WM_TIMER documentation](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-timer)
explains the message's low priority. The fix reduces per-frame work and respects the
system preferences; it does not change the global timer resolution.

### Expanded effects

Measurement with the new effects recorded 88/85 paints in 1.4 s, normal/compact.
Median intervals: 15.45/16.36 ms; 95th percentile: 18.91/21.06 ms. Portal drawing
with energy: 9.07/12.00 ms. System load varies between measurements; the evidence
remains limited to the test host. Release built without warnings, with the six
suites passing in 47.76 s.

The new tests exercise landscape hover, mouse leave, compression and ripple on a
native BUTTON, cancelling the gesture without playing, stage confirmation and
removal of the effects under reduced motion. The current preview has 167
deterministic frames, at 20 ms during motion; it includes the opening, portal and
button interaction, selection, transition and stage completion. States and events
are simulated; there is no real installation or monitor recording.

### Navigation, button and atmosphere

In the current delivery, the portal recorded 89/90 paints in 1.4 s at normal/compact
sizes: median intervals of 15.39/15.01 ms, p95 of 19.62/21.20 ms. Drawing with
particles had a median of 8.53/7.36 ms. Navigation with moving HWNDs recorded 70/76
paints in 1.4 s: medians of 19.08/17.00 ms and p95 of 26.85/31.13 ms. Page changes
involve more native work; a constant 60 FPS is not promised. Settings drawing had a
median of 10.54/9.40 ms.

Release `/W4` without warnings, six suites passing in 47.87 s. The new regressions
check entering/returning at both sizes, Tab during motion, preservation of focus,
font and edited text, repeated reopening, hit targets at the end, immediate switch
under reduced motion and presentation independent of the bitmap's DPI.

The preview host forces native painting of the client and each child after updating
every HWND. `PrintWindow` returned a stale frame after shrinking the window in this
environment. The capture checks pixel samples to confirm the header is the current
frame at the correct scale. This does not change how the launcher runs.

### Settings entrance optimization

The form is now rasterized at its final position once during the entrance. Motion
frames copy the fixed header and shift the body by whole pixels, reusing the drawn
text, lines and controls. EDITs, BUTTONs and the TRACKBAR remain native and follow
the motion. Editing, focus, hover, messages and state changes invalidate the cache
immediately. The temporary image is freed when finishing, leaving, resizing,
hiding/minimizing or reducing motion. The look, durations and animation curves are
preserved.

Local before/after measurement, on 2026-10-04, with the same benchmark:

| Measure | Before | After |
|---|---:|---:|
| Settings drawing, normal | 9.45 ms | 2.65 ms |
| Settings drawing, compact | 9.61 ms | 2.38 ms |
| Navigation interval, normal, median | 19.05 ms | 16.31 ms |
| Navigation interval, compact, median | 17.78 ms | 15.02 ms |
| Navigation paints in 1.4 s, normal/compact | 69/76 | 80/83 |

Navigation p95 was 31.09/24.09 ms after the change (24.18/35.83 ms before). Opening
and returning still include initial preparation and landscape drawing; the intervals
come from the native host, without guaranteeing physical FPS or removing every spike.
The form median considers eight warm-up frames.

Release `/W4` without warnings; six suites passing in 47.89 s. The new tests check
reuse, invalidation on edit/focus, equivalence with the uncached form and release
on leave/finish. The normal/compact captures were reviewed; the host also fixes the
frame offset that `WM_PRINT/PRF_CHILDREN` applied to children, painting each control
at the correct client origin.
