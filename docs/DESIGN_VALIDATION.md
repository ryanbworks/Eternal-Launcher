# Redesign validation

Direction confirmed by the owner: native C++ launcher for the private server, a
futuristic dark theme with violet, implemented directly in code.

## Installation tracking — 2026-10-04

MSVC Release `/W4` with no warnings in `out/build/installation`; all six CTest suites
passed, including AutoUpdate and the new installation tests. The visible native
host confirmed Tab and Enter on the tracking controls at normal and compact sizes.
The captures use synthetic data; no full installation from the public services and
no real Minecraft/Forge run was performed. See the
[implementation and validation record](INSTALLATION_TRACKING.md) and the review in
`.impeccable/review/installation/review.md`.

## Implementation

- Landscape layout on the left and a continuous access console on the right.
- Vector E/portal mark, multi-resolution icon, artwork and font embedded in the EXE.
- Custom Win32 buttons, a native EDIT field and visible focus.
- Keyboard order: Nickname local → Original → nickname → Play → Settings; Original hides the field.
- Hover transitions of about 180 ms and progress interpolation. The Windows client-area animation preference disables motion; the timer stops when there is no transition or indeterminate operation.
- Resizable window with rendering scaled by size/DPI. Logical composition of 1180 × 730.
- Dedicated states for first installation, authentication, authentication failure and invalid nickname. Errors use a shared classification in the action/activity panel.
- Installation, manifest, Java, Microsoft, download, sync and launch services preserved.

## Evidence

MSVC Release /W4 build with no warnings. CTest: MinecraftLauncherServiceTests, 1/1 passed.

Ten deterministic renders from the final renderer, including 1180 × 730 and 944 × 584, with no clipping observed. The scenes use illustrative data.

On the first compiled version, Computer Use checked the real window, Original access by click, hiding the field, Tab and Shift+Tab between controls and returning to local mode with Enter. The user stopped the control with Esc; there was no further window automation. The Tab order fixed in the final version was reviewed in code, without a new real-window test.

Microsoft login, file installation and launching Minecraft were not run in this task. Full DPI transitions, the reduced-motion preference and animation cadence were not exercised live. The settings screen was implemented in the extension recorded below. News is explicitly labeled as a demo.

The initial independent review and the fix review are in `.impeccable/review/`. The visual system is documented in `DESIGN.md` and `.impeccable/design.json`.

## Extension: Minecraft settings

A functional screen in the existing visual system, with initial/maximum RAM,
resolution, fullscreen, automatic/custom Java, a dedicated game folder, minimize
while playing and shortcuts to the saved instance. Only Save persists the form;
reset is a draft. Installation, login and a running game lock changes.

MSVC Release /W4 with no warnings in `out/build/settings`. CTest: **3/3 passed**.
`MinecraftLauncherServiceTests` keeps the existing regressions;
`LauncherSettingsTests` checks UTF-8 JSON, defaults and a preserved invalid file,
invalid saves preserving data, RAM/resolution/fullscreen in the command, Java 17 and
folder protection. `SettingsPageTests` uses a hidden Win32 host and real EDIT/BUTTON
controls to check editing, options, locks, validation, restore and draft disposal
on reopening.

Captures in `.impeccable/review/settings/`: normal 1180 × 730, compact 944 × 584,
an error with initial RAM 8192 and maximum 4096 MiB, plus the preserved main screen.
They are deterministic renders of the production code, with native fields shown as
drawn text. They don't prove live focus/Tab, selection/caret, the file/folder
picker, DPI or real Minecraft behavior.

There was no desktop control after the earlier Esc interruption. No login, download,
installation or game start was run in this extension. The minimize/restore option
was inspected in code, without exercising it with the real Minecraft. The independent
review and its verdict on the fixes are in the same review folder.

## Fix: light flashes on hover

The `WindowPaintingTests` test reproduced the button background being erased with
`#F0F0F0` on both screens before the fix. The default BUTTON procedure ran before
the custom drawing. The earlier static previews didn't show that step of the paint
cycle.

Buttons now keep their pixels on `WM_ERASEBKGND`; the fallback brush is dark. The
window includes the native controls in its presentation with `WS_EX_COMPOSITED`.
Hover/focus invalidate the canvas and the custom buttons, without asking the EDIT
fields to repaint on every movement. Changes on the settings screen notify the same
paint controller as the main screen. A frame marked as stale is rendered and
finished before any copy to the controls, even when `WM_DRAWITEM` comes before
`WM_PAINT`.

MSVC Release /W4 with no warnings in `out/build/paint-fix`; CTest **4/4 passed**.
The regression test uses the production WindowProc, real Win32 controls in a hidden
host and an in-memory HDC. It confirms a dark background on both screens, up-to-date
frames on hover entry before WM_PAINT and corner coverage across 24 hover toggles.
The previous result had four failures; the new one has zero. No user window was
controlled.

Pixel-by-pixel comparison of the main, compact, settings, compact settings and error
previews: all identical to the captures approved before this fix. Layout and palette
were preserved. These tests check the native paint path; they don't measure the
presentation cadence on the physical screen.

## Extension: RAM slider and default minimize

Maximum RAM gets a native TRACKBAR with 512 MiB (0.5 GB) steps, a GB readout with a
decimal comma and 4, 6 and 8 GB shortcuts. The MiB fields keep precise integer
entries. The initial 0.5–16 GB range grows for larger values; lowering it with the
slider or a shortcut also lowers an initial RAM that exceeds the new maximum. The
minimize option is on in the defaults and after restoring; a preference explicitly
saved as off is still respected.

MSVC Release /W4 with no warnings in `out/build/ram-slider`; CTest **4/4 passed**.
The hidden host uses the native controls and the production screen controller. It
checks 1/1.5/2/4.5/6.5 GB, exact JVM arguments, arrows/Page Down, shortcuts, precise
editing, the extended range, locks, restore and dark painting. The preference tests
check minimize on with no file or without the property, persistence of an off choice
and restoring the option on screen.

Final captures in `.impeccable/review/ram-slider/`: normal 1180 × 730, compact
944 × 584, a 6.5 GB/6656 MiB selection and a RAM limits error. All show the default
minimize option checked. They are deterministic GDI+ renders with illustrative data;
they don't prove physical dragging, on-screen cadence or starting Minecraft. The
independent review is in the same folder.
