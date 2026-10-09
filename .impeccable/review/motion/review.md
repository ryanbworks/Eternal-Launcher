# Motion finish review — 2026-10-04

Disposition: ship within the requested native motion scope. Self-review, not an
independent reviewer or an actual Minecraft installation.

Direction: portal aperture on first opening; continuity via a left-body shutter;
immediate native actions; interpolated identity, transfer fill and installation
button hover. Existing graphite/violet tokens, fonts, content and control bounds
are preserved. No perpetual decoration, new dependency or inaccessible custom input.

Inspection pass 1 reviewed portal, identity and installation at normal/compact
sizes. The old/new copy overlapped in the initial transition; a single fix batch
replaced the crossfade with a shutter and aligned the synthetic fixture's summary.
Pass 2 confirmed sharp non-overlapping copy, visible actions and the completed
normal/compact layouts. No further visual polishing was performed.

Validation: Release `/W4` builds without warnings. All six CTest suites passed
(48.49 seconds); after the shutter adjustment, the affected WindowPaintingTests
suite passed again (1.87 seconds). New assertions cover reversing selection,
scene interruption, cache release, native cancel availability, transfer reset,
instant reduced-motion feedback and hidden/minimized timer suspension.
The design detector returned `[]`; it does not analyze native GDI+ semantics.
Scoped `git diff --check` passed. No commit, release or publication was performed.

Evidence: PNG samples from production rendering at 1180 × 730 / 944 × 584 and GIF
previews encoded from those frames. Host state and nickname are synthetic.
Desktop frame cadence, physical pointer interaction, monitor transitions and
screen-reader behavior were not measured. The test injects reduced motion rather
than changing the Windows preference. Source details are in docs/ANIMACOES.md.

## Performance follow-up

The owner reported choppy animation. Baseline warm drawing was 64.32ms per home
frame and visible-host WM_PAINT intervals were 72.63ms. Cached, premultiplied,
display-size image rasters remove the repeated expensive image resamples. Animation
frames skip control synchronization and use a high-resolution monotonic clock;
the timer requests USER_TIMER_MINIMUM without global timer-resolution changes.

Final warm drawing: 10.86ms home / 10.76ms portal / 7.15ms installation, normal.
Visible-host intervals: median 15.40ms normal / 15.20ms compact; p95 19.04 / 21.16ms.
The benchmark received 88/90 WM_PAINTs over 1.4 seconds. These are window paint
measurements, not captured physical display FPS. Cold cache work is excluded.

Release builds without warnings; all 6 CTest suites passed in 47.67 seconds.
Raster tests verify reuse and scale-triggered regeneration. One batched visual
inspection confirmed normal/compact image quality and installation controls.
Detector returned `[]`, with the same native-analysis limitation. Scoped diff
checks passed. GIF sampling increased to 20ms, eliminating coarse state steps in
the preview. No publication or system preference change was performed.

## Expanded interaction motion

The owner explicitly requested more visible motion. Added portal ribbons/particles
on hover and bounded startup activation, a Play sweep/compression/ripple, gear
rotation and active/completed-stage marker feedback. Cached imagery and fixed
native hit regions are retained. Reduced motion removes the spatial effects;
pointer exit and finite feedback stop the timer. Repeated gestures remain cancellable.

One batched visual pass inspected normal/compact portal activity, Play hover/press,
startup and stage confirmation. Labels, metrics and native control bounds remain
clear. No correction or second visual polishing pass was needed. Release `/W4`
built without warnings; all six suites passed in 47.76s. New tests cover hover
activation/exit, press cancellation without launch, finite confirmation and
reduced-motion effect removal. Detector returned `[]` with native limitations;
scoped diff checks passed.

Visible-host benchmark with energy activity: 88/85 paints in 1.4s, normal/compact;
median intervals 15.45/16.36ms, p95 18.91/21.06ms. Warm portal draw 9.07/12.00ms.
These host measurements do not guarantee physical compositor FPS on every machine.
The updated 134-frame GIF uses simulated time samples, not a desktop recording.

## Settings navigation and atmospheric motion

The latest owner request explicitly authorizes visible page transitions, a richer
Play button and more particles. Settings slides right-to-rest in 340ms and home
left-to-rest in 280ms. Native controls follow in a batch, preserving drafts, focus,
fonts, keyboard order and hit targets. The header stays fixed. Play gains a soft
sweep, cursor-following light, moving edge reflection, stronger icon/press feedback.
64 particles have differing depth, size and speed. Foreground home runs atmosphere;
settled settings, hidden/minimized windows and reduced motion stop that work.

Self-review, not independent review. Two batched visual inspections checked
normal/compact button feedback, particles and navigation. The compact native
capture exposed stale PrintWindow content. The fixture now uses synchronous
WM_PRINT with real children, verifies header pixels against the current frame,
and uses the launcher's DPI awareness. All 36 settings capture checks pass;
the fixed header also matches the home buffer outside the hidden gear. Production
presentation now supplies explicit pixel bounds rather than relying on bitmap DPI.
No additional aesthetic refinement was performed after the two inspections.

Release `/W4` built without warnings; all six CTest suites passed in 47.87s.
New native regressions cover both sizes, editing/focus/font preservation, Tab
during entry, repeated opening, return targets and reduced motion. Pixel tests
verify presentation even with changed bitmap resolution. Detector returned `[]`
with the same GDI+ limitations; scoped whitespace checks passed.

Final portal cadence: 89/90 paints per 1.4s, median 15.39/15.01ms and p95
19.62/21.20ms. Navigation cadence: 70/76 paints, median 19.08/17.00ms and p95
26.85/31.13ms, normal/compact. Portal draw: 8.53/7.36ms; settings draw:
10.54/9.40ms. These are test-host paint timings, not compositor FPS guarantees.
The 167-frame GIF uses deterministic time samples and native settings rendering,
not recorded physical input. No release, publication or preference change.

## Settings-entry performance follow-up

The approved appearance is preserved; settings timer frames now reuse a current
form raster, invalidated immediately by native interaction/state changes.
Warm form work is 2.65/2.38ms instead of 9.45/9.61ms. Median native navigation
intervals are 16.31/15.02ms, normal/compact. All six suites pass (47.89s).
The native capture fixture now prints children at explicit client origins,
correcting the previous WM_PRINT/PRF_CHILDREN offset. The GIF's settings frames
were refreshed accordingly. Full evidence and limits are in
`../navigation-performance/review.md` and `docs/ANIMACOES.md`.
No publication or system preference change was performed.
