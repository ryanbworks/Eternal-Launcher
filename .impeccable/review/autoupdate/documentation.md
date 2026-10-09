# AutoUpdate documentation handoff

Date: 2026-10-03. Scope: ordinary extension of the existing native Windows Win32/GDI+ launcher. The independent finish review in `review.md` has disposition **ship** for the update phases, recovery action, and version identity. No design-system change was requested or found necessary. `DESIGN.md`, `.impeccable/design.json`, `PRODUCT.md`, and the surface brief were preserved.

## Evidence checked

- Product and design authority: `PRODUCT.md`, `DESIGN.md`, `.impeccable/design.json`, and `.impeccable/surfaces/src-pages-mainpage-cpp.md`.
- Implementation: `src/pages/MainPage.cpp`, `src/components/ProgressComponent.cpp`, `src/components/HeaderComponent.cpp`, `src/styles/Theme.h`, `src/config/LauncherConfig.h`, and `src/config/LauncherVersion.h.in`.
- Review: `.impeccable/review/autoupdate/review.md`, including its evidence limitations.
- Independently opened all seven full-canvas update captures: `update-check.png`, `update-download.png`, `update-apply.png`, `update-restart.png`, and `update-recovery.png` at 1180 × 730; `update-compact.png` and `update-recovery-compact.png` at 944 × 584. Their phase labels, percentages, version label, and recovery copy match their names and inspected source. No clipped or overlapping added copy was observed.

## Incumbent system comparison

The fifteen `Theme.h` colors match the normative color values in `DESIGN.md` and the sidecar's canonical metadata. The extension continues to use the existing primary, secondary, muted, violet, and lilac roles. It adds no shared color, type, radius, spacing, shadow, or motion token.

The header and hero retain Sora; operation and activity copy retain Segoe UI. Activity uses the existing 13-unit bold heading, 12-unit bold percentage, and 11-unit supporting text. The manual action adds an underlined 11-unit lilac label within that small-copy role. Its local native focus outline and hit bounds are component details, not a replacement type or shape system.

The centered 1180 × 730 logical canvas, uniformly scaled compact view, left landscape, three flat installation columns, and right 336 × 592 continuous console remain intact. Four updater phases occupy the existing operation region and progress rail: checking, downloading, applying, and reopening. Their native controls lock while the update is active. Header version text reads `v1.0.0-Aurora` in all seven captures and is supplied through `LauncherConfig`.

After a failed update, the source retains the warning and resumes instance checking. When other work is idle, the activity region shows “Atualização pendente” and “Baixar launcher”; the recovery captures retain the enabled violet “JOGAR AGORA” action. The warning area and link area fit independently at both supplied sizes. The manual action is a real owner-drawn Win32 BUTTON with the matching accessible label, `WS_TABSTOP`, scaled bounds, explicit Enter activation, and a lilac focus outline. Its click handler opens `LauncherConfig::LauncherDownloadUrl` through `ShellExecuteW`.

Existing motion remains time-based: 35 ms hover and 85 ms forward-progress constants, immediate progress reset at a new phase, a 1600 ms indeterminate cycle, and a 16 ms timer request that stops when settled or minimized. The Windows client-area animation preference still suppresses transitions. The sidecar accurately distinguishes its 180 ms CSS approximation from this native runtime behavior.

## Five-line system summary

1. World: graphite and indigo portal console with restrained violet emphasis and the existing voxel landscape.
2. Typography: Sora display identity and Segoe UI native controls and operational copy.
3. Geometry: centered, uniformly scaled desktop canvas with the established continuous access console and flat status groups.
4. Interaction: phase-specific update progress plus a quiet native recovery action that preserves the game action after recoverable failure.
5. Motion: existing activity-driven easing, indeterminate progress, timer stopping, and Windows reduced-motion behavior.

## Limits and preserved drift

These are deterministic production-renderer captures with synthetic states and nickname text, without the native window frame. They establish composition and copy fit; source establishes the intended native input paths. This documentation pass did not run the updater, activate the link, test live keyboard/focus behavior, measure desktop cadence, or verify DPI transitions. The test and cancellation results in the review are implementation-handoff evidence, not tests rerun by the documenter. No web detector was used. The context CLI's reported lack of Win32 support does not reclassify this application as web, iOS, or Android.

Pre-existing drift was left unchanged: the surface brief says a 340-unit console and 180 ms native fades, while source and `DESIGN.md` use a 336-unit console and time-based 35 ms hover easing. The new recovery control extends the historical tab-order description and is recorded here. This scoped evidence record documents the extension without refreshing the incumbent design files.
