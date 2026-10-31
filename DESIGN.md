---
name: Eternal Launcher
description: Dark native Windows launcher with a violet portal identity.
colors:
  background: "#0d0e14"
  surface: "#161721"
  surface-elevated: "#1e1f2c"
  surface-hover: "#2a283b"
  border: "#343446"
  accent: "#a582ff"
  accent-light: "#c9b6ff"
  accent-ink: "#1a1031"
  success: "#37d690"
  warning: "#fbbf24"
  error: "#f87171"
  information: "#60a5fa"
  text-primary: "#f5f7fa"
  text-secondary: "#b2b3c3"
  text-muted: "#9697ac"
typography:
  display:
    fontFamily: "Sora, Segoe UI"
    fontSize: "35px"
    fontWeight: 700
  wordmark:
    fontFamily: "Sora, Segoe UI"
    fontSize: "26px"
    fontWeight: 700
  headline:
    fontFamily: "Segoe UI"
    fontSize: "22px"
    fontWeight: 700
  title:
    fontFamily: "Segoe UI"
    fontSize: "18px"
    fontWeight: 700
  status-title:
    fontFamily: "Segoe UI"
    fontSize: "17px"
    fontWeight: 700
  body:
    fontFamily: "Segoe UI"
    fontSize: "13px"
    fontWeight: 400
  body-small:
    fontFamily: "Segoe UI"
    fontSize: "12px"
    fontWeight: 400
  label:
    fontFamily: "Segoe UI"
    fontSize: "12px"
    fontWeight: 700
  action:
    fontFamily: "Segoe UI"
    fontSize: "13px"
    fontWeight: 700
  input:
    fontFamily: "Segoe UI"
    fontSize: "15px"
    fontWeight: 400
  micro:
    fontFamily: "Segoe UI"
    fontSize: "11px"
    fontWeight: 400
  category:
    fontFamily: "Segoe UI"
    fontSize: "10px"
    fontWeight: 700
rounded:
  panel: "14px"
  control: "10px"
  tab: "8px"
  track: "2px"
spacing:
  tab-inset: "4px"
  field-inset: "14px"
  news-gap: "16px"
  console-inset: "24px"
  hero-inset: "32px"
  outer-inset: "36px"
components:
  button-primary:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.accent-ink}"
    typography: "{typography.action}"
    rounded: "{rounded.control}"
    width: "288px"
    height: "54px"
  button-primary-hover:
    backgroundColor: "{colors.accent-light}"
    textColor: "{colors.accent-ink}"
  button-primary-active:
    backgroundColor: "{colors.accent-light}"
    textColor: "{colors.accent-ink}"
  button-primary-disabled:
    backgroundColor: "{colors.surface-hover}"
    textColor: "{colors.text-secondary}"
    typography: "{typography.action}"
    rounded: "{rounded.control}"
    width: "288px"
    height: "54px"
  identity-selected:
    backgroundColor: "{colors.surface-hover}"
    textColor: "{colors.accent-light}"
    typography: "{typography.label}"
    rounded: "{rounded.tab}"
    height: "32px"
  identity-unselected:
    backgroundColor: "{colors.background}"
    textColor: "{colors.text-secondary}"
    typography: "{typography.label}"
    rounded: "{rounded.tab}"
    height: "32px"
  nickname-field:
    backgroundColor: "{colors.surface-elevated}"
    textColor: "{colors.text-primary}"
    typography: "{typography.input}"
    rounded: "{rounded.control}"
    width: "288px"
    height: "44px"
  access-console:
    backgroundColor: "{colors.surface}"
    rounded: "{rounded.panel}"
    padding: "24px"
    width: "336px"
    height: "592px"
  settings-button:
    backgroundColor: "{colors.background}"
    textColor: "{colors.text-secondary}"
    rounded: "{rounded.control}"
    width: "44px"
    height: "44px"
---

# Design System: Eternal Launcher

## Installation tracking extension

The main page remains Operate mode. During game preparation, error or cancellation,
`InstallationPanelComponent` occupies the existing left area (36,112,744,592).
It uses the same indigo surface, Segoe UI operational typography, violet active
selection and semantic status colors. Six continuous rows replace the cinematic
landscape, installation columns and news for this state only. The right access
console stays in place and shows the current stage and disabled primary action.

The current row uses a tonal selection and a readable state label. Below the
rows are the actual activity, transfer bytes/optional percentage, a flat progress
track, speed/conditional ETA and verified/reused counts. Checking, hashing,
extraction and Forge execution use an indeterminate track. Errors and cancellation
replace transfer details with recovery copy; the primary action becomes Retry or
Continue. No global weighted percentage or time estimate is invented.

Abrir log and Cancelar instalação are real owner-drawn Win32 BUTTON controls
with hover, pressed, disabled and focus states. Their Tab order is log then cancel;
Enter and Space retain native activation. Cancelling disables repeat cancellation.
Windows reduced-motion preference fixes the indeterminate position. The ordinary
landscape returns once Minecraft starts successfully.

Evidence: `.impeccable/review/installation/` includes seven synthetic production
states and two PrintWindow captures from a visible test-owned native host at
1180 × 730 and 944 × 584. The native host confirmed Tab and Enter with real EDIT
and BUTTON controls, using synthetic transfer values and Steve. Automated tests
exercise HTTP against a loopback fixture and game-launch decisions against doubles.
These captures do not demonstrate a public-service installation, real Forge or
Minecraft execution, physical pointer interaction, monitor DPI transitions or
screen-reader announcements. See `docs/INSTALLATION_TRACKING.md` for scope.

## Overview

**Creative North Star: "Portal observation console"**

Eternal uses graphite and indigo surfaces, violet active controls and the hexagon-and-star emblem to give a private Minecraft launcher a dark, futuristic identity. A cinematic voxel landscape supplies atmosphere; a calm access console makes identity, installation readiness and the next action easy to read.

The system is a native Windows application built with Win32 and GDI+. Sora Bold gives the wordmark and hero a distinct voice, while Segoe UI keeps controls and operational copy familiar and compact. Tonal layers, precise hairlines and generous separation organize the information without shadows or ornamental motion.

**Key Characteristics:**

- Graphite field, indigo layers and restrained violet emphasis.
- Sora display type paired with Segoe UI for native interaction.
- One environmental illustration and a geometric portal monogram.
- Continuous access surfaces and flat status groups.
- Semantic states, visible keyboard focus and motion tied to activity.

This record describes the built system in `src/styles/Theme.h`, `src/components/`, `src/pages/MainPage.cpp`, `src/pages/SettingsPage.cpp` and `src/utils/DrawingUtils.cpp`. The frontmatter is normative; dimensions are logical pixel units before the client-area transform.

## Colors

Violet gives the graphite and indigo palette its identity, while service states retain distinct, readable semantic colors.

### Primary

- **Portal Violet** (`accent`): portal monogram and enabled primary action; also the active progress fill.
- **Pale Lilac** (`accent-light`): hover and pressed action, selected identity text, version details and focus outlines.
- **Violet Ink** (`accent-ink`): readable action text and the inset primary-action focus ring.

### Neutral

- **Graphite Ground** (`background`): client-area background and segmented-control well.
- **Indigo Surface** (`surface`): access console and image fallback.
- **Raised Indigo** (`surface-elevated`): nickname field and unselected-tab hover endpoint.
- **Muted Violet Surface** (`surface-hover`): selected identity tab, disabled primary action and progress track.
- **Indigo Hairline** (`border`): quiet separators and resting field outline.
- **Cool White** (`text-primary`): display, headings, input text and status values.
- **Soft Silver** (`text-secondary`): explanations and supporting operational copy.
- **Quiet Silver** (`text-muted`): metadata, helper text and compact status labels.

### Semantic States

- **Mint Success** (`success`): ready actions and confirmed healthy results.
- **Amber Attention** (`warning`): incomplete installation and caution.
- **Coral Error** (`error`): authentication, validation, connectivity, file or launch failure.
- **Clear Blue Information** (`information`): informational status-card results.
- Neutral status-card dots use `text-muted`; active non-ready operations use `accent`.

**The Action Violet Rule.** Reserve violet emphasis for identity, selection, focus and the primary action; use semantic state colors to explain service results.

The artwork contains additional environmental colors and translucent dark overlays; these are image composition details, not shared interface tokens. Sidecar tonal ramps are generated inspection aids, not additional application palette values.

## Typography

**Display Font:** Sora Bold, with Segoe UI Bold fallback.
**Body and Control Font:** Segoe UI.

Sora's geometric construction echoes the portal mark. Segoe UI supports compact Portuguese operational copy and native editing. GDI+ text uses pixel-sized fonts and native font metrics; the implementation sets no custom tracking or line-height tokens.

### Hierarchy

- **Display:** `display` for the multiline hero; `wordmark` for the Eternal name.
- **Headline:** `headline` for the access heading.
- **Title:** `title` for installation; `status-title` for action readiness and status values.
- **Body:** `body` for hero explanation and Microsoft account text; `body-small` for instructions and operational messages.
- **Labels:** `label` for identity modes and field labels; `action` for the uppercase primary action.
- **Input:** `input` for the native nickname edit.
- **Small copy:** `micro` for helpers, activity text and news provenance; `category` for compact status labels.

The news heading and product name use a local Segoe UI Bold size between body and status title (15 logical units). They do not establish another shared type role.

Sora Bold is embedded as a private GDI+ font from `assets/fonts/Sora-Bold.ttf` under the bundled OFL license. If loading fails, the display helper falls back to Segoe UI Bold. Layout recreates the native edit's Segoe UI font at the scaled input size.

**The Display and Control Rule.** Use the embedded Sora Bold for the hero and wordmark. Keep interactive controls, body copy and status text in Segoe UI.

## Layout

The current desktop composition uses a centered logical canvas (1180 × 730). The outer horizontal inset uses `outer-inset`; the main content begins below the header divider.

The main page places a landscape (744 × 360) at (36, 112), followed by three flat equal-width installation columns and a quiet news row. A continuous access console (336 × 592) starts at (808, 112). Its `console-inset` leaves a 288-unit control width. The gap between landscape and console is 28 units. These coordinates describe this page, not a mandatory composition for every future surface.

Layout uniformly scales to fit the client area using the smaller width/height ratio and centers the remaining space. It does not reflow into stacked columns. Native child control bounds follow the same transform; floor/ceiling rounding covers button edges. The actual minimum tracking size is an outer window size (900 × 590), from `WM_GETMINMAXINFO`; the unused minimum constants are not design tokens. DPI changes use Windows' suggested window rectangle.

The compact review capture is a scaled logical canvas (944 × 584 at 0.8 scale). It verifies rendered alignment; it does not establish a breakpoint or minimum readable physical type size.

## Elevation & Depth

Interface depth uses tonal layers, restrained hairlines and the landscape's atmosphere. No interface drop shadows are implemented. The hero clips the artwork to its panel silhouette, crops it centrally to cover, and adds a left-to-right dark translucent gradient for legible display copy. The rest of the interface remains flat.

**The Quiet Depth Rule.** Use tonal surfaces, hairlines and the artwork's atmospheric depth. Do not add interface drop shadows to this system.

## Shapes

Panels use `panel` rounding; controls and the field use `control`; identity segments use `tab`; the progress rail uses `track`. Rounded geometry clamps its diameter to the available bounds. Resting field outlines and separators are one logical unit; field/tab/settings focus uses a two-unit lilac outline.

Icons and the play triangle are authored vector geometry, not text glyphs. The logo is the emblem in `assets/lavender-cosmic-portal.png`, shared by the native ICO, the title bar and Settings › About. The play mark appears only for the ready-to-play operation.

## Components

### Primary Action

The enabled action is a violet, rounded rectangle with dark ink and an uppercase label. Its observed height is the `button-primary` height. Readiness uses “JOGAR AGORA”; incomplete installation uses “INSTALAR E JOGAR”; recoverable errors use “TENTAR NOVAMENTE”; active operations show an appropriate busy label.

Hover interpolates to pale lilac; press uses the lilac endpoint. Keyboard focus draws a dark inset outline (three-unit inset, two-unit stroke, seven-unit radius). Disabled actions use the muted violet surface and secondary text. The underlying control remains a Win32 owner-drawn BUTTON with a synchronized accessible label.

### Identity Navigation

Two native buttons occupy a graphite segmented well (40 units high). The local nickname segment receives 56% of the usable width; Original receives 44%. The selected segment uses the muted violet surface and lilac label. Hover on an unselected segment interpolates toward raised indigo. Selection and focus remain separate visible treatments.

### Nickname Field

The raised indigo field has a quiet border, internal `field-inset` and native Segoe UI editing. Focus changes the outline to lilac. Native selection, clipboard and horizontal scrolling remain available. The limit is 16 characters and the adjacent helper describes the accepted range. Validation failure focuses the edit and explains the correction in the operation area; it does not introduce an unimplemented red field variant.

Original mode hides the nickname EDIT and renders the Microsoft account name or sign-in guidance inside the same field silhouette. That display is not an editable account textbox.

### Access Console and Activity

One indigo container groups access, operation explanation, primary action and activity. Dividers establish hierarchy within it. Ready and busy messages follow actual semantic operations. A warning dot identifies normal installation need; error dots and the “Atenção necessária” activity heading cover the error operations.

Activity displays a four-unit progress rail only while work is active. Percent labels report the actual integer result; the rail eases toward it. Zero-percent active checks use a travelling segment clipped to the rail. Idle shows an explanatory activity message without a rail.

### Installation Status and News

Three flat columns use semantic dots, small labels, bold values and supporting details. Vertical hairlines separate columns; they are not individually elevated cards. Status concerns installation, files and the HTTP manifest service, not live gameplay availability.

News remains a quiet heading and description under a divider. “Notícia de demonstração” explicitly identifies the sample. The hero artwork in `assets/realm.png` is generated illustrative concept art, not a private-server screenshot; its exact prompt is stored in `assets/realm.prompt.txt` and image metadata.

### Settings and Native Interaction

Settings uses an authored gear in a square control. Hover adds a muted surface; focus adds a lilac outline. It opens the functional native settings page, preserving the shared header and client-area transform.

The settings surface uses two continuous form columns: performance and game window at left, Java and instance files at right. Back precedes the heading; a bottom hairline separates validation feedback from Restore defaults, Cancel and the violet Save action. This composition is specific to settings. It extends the existing tonal, flat control language without adding shared palette, typography or spacing tokens.

Raised indigo fields retain quiet hairlines and lilac focus outlines, with real Win32 EDIT controls inset into the GDI+ silhouettes. Settings text edits use a local scaled Segoe UI size (14 logical units); group headings use a local bold size (20). They do not replace the shared type ramp. Native owner-drawn BUTTON controls provide Java mode selection, folder actions, footer actions and checkbox-style toggles. Selected Java uses muted violet surface and lilac text; checked toggles use authored check geometry and violet fill. Save uses violet with dark ink, a lilac hover/pressed endpoint and an inset dark focus outline. Secondary actions remain indigo. Disabled text uses the existing muted role.

Maximum RAM selection leads the left form with a 32-unit native TRACKBAR, a six-unit violet rail and an 18-unit round thumb. A value label displays GB with a Portuguese decimal comma; three indigo shortcuts offer 4, 6 and 8 GB, using the existing selected surface and lilac text. The slider keeps native mouse/arrow-key behavior while its paint copies the finished GDI+ frame, preserving the dark repaint correction. Numeric MiB fields remain below for precise input, followed by window preferences. These are local component dimensions, not new shared tokens.

The rail starts at 0,5–16 GB in 512 MiB steps and expands for larger saved or manually typed allocations. The displayed convention is 1 GB = 1024 MiB. Exact integer MiB edits remain unchanged by synchronization; selecting the slider or a preset lowers an initial value above the newly selected maximum. MainPage forwards native WM_HSCROLL notifications. The thumb uses lilac on hover/focus, a lilac outline marks slider focus, and operation locks mute the rail/thumb and disable the slider and presets. No new motion vocabulary is introduced.

The “Minimizar launcher ao jogar” checkbox is marked for default settings and Restore defaults. A previously saved false value remains unmarked. The checked violet square and authored dark check follow the existing toggle treatment; reset remains a draft operation requiring Save.

RAM and resolution accept integer values and report validation guidance in the footer. Automatic Java disables its custom path controls; fullscreen disables window dimension editing. Installation, login and game execution disable editing and Save while Back, Cancel and saved-instance folder shortcuts remain available. Native control labels include selected state. Values persist only through Save to UTF-8 JSON; Restore defaults changes the draft, and Cancel, Back or Escape return without saving. Reopening reloads the saved values. Changing the exclusive instance path prepares another instance and leaves worlds in the previous folder; the visible helper and game/mods/logs shortcuts explicitly refer to the saved instance.

The native title bar, resize behavior and window buttons remain Windows controls. Main-page dialog tab order is identity modes → visible nickname → primary action → settings. Settings tab order is explicitly set to Back → RAM slider → 4/6/8 GB shortcuts → RAM edits → window options → Java and instance options → footer, skipping disabled controls. Arrow keys change the focused slider by 0.5 GB; Page Up/Down change it by 1 GB. Enter activates the focused button or submits a settings edit through Save; Escape returns to the main page. Main-page Enter submits the nickname; native button keyboard behavior is retained.

### Motion

The focal entrance opens the artwork around the portal in 750ms using a quadratic deceleration and a clipped aperture. Headings and controls remain visible and usable. It plays once at startup; returning from settings does not replay it. The left body changes between the landscape and installation with a 240ms shutter reveal from cached presented pixels. The reveal keeps old and new copy from overlapping and excludes the native log/cancel area. Rapid scene changes restart from the last rendered image; completion, resize and reduced motion release the snapshot.

The owner's request for more visible motion adds 24 portal particles and 40 atmospheric particles across three depth layers, confined to the artwork side. Their size, speed and trails differ; an eight-second drift base runs while home is foreground. Two orbital ribbons respond more strongly to landscape hover and a bounded startup activation. Play has a soft 480ms sweep on hover/keyboard focus, a cursor-following light with 55ms smoothing, a moving edge reflection, a 10px icon movement/trail, 3px press compression within the native hit area and a 380ms click/Space ripple. The gear rotates 60 degrees with hover. Active installation stages have an orbiting marker; completed/available stages have a check and a 420ms confirmation pulse. Reduced motion removes energy, particles, sweeps, ripples and pulses while preserving immediate feedback and state labels.

Settings enters from a 96px right offset in 340ms; home returns from a 72px left offset in 280ms, both with cubic deceleration. The header stays fixed. Real EDIT/BUTTON/TRACKBAR HWNDs follow the pixel-rounded canvas translation in a DeferWindowPos batch, without recreating fonts. Focus, Tab, editing and hit targets stay aligned throughout. Repeated open does not reset the draft or entrance; resize and reduced motion snap to the final layout.

Settings entry reuses a premultiplied raster of the current form at rest, copying the fixed header and translating only the body by integer pixels. Timer frames avoid repeated text/vector drawing. Native interaction and state changes invalidate the raster immediately; settling, page exit, resize, hidden/minimized state and reduced motion release it. The approved appearance, geometry and motion timings are unchanged. Native focus/editing remains live rather than included as fake input text in the cache.

Feedback uses time-based exponential easing: main hover has a 35ms time constant, installation hover 45ms, identity selection 55ms, installation fill 70ms and ordinary progress 85ms. The identity indicator moves between the actual segment bounds while the account form and accessible selection update immediately. The transfer bar follows the actual percentage; its text and byte counters are never animated or estimated. A new stage, attempt or transfer resets its fill. The indeterminate segment has a 1600ms cycle. A timer requests USER_TIMER_MINIMUM (10ms) during transitions, indeterminate work or foreground home atmosphere. It stops when hidden, minimized, closing or settled on settings. Atmosphere stops scheduling on foreground loss, while any finite feedback can finish. Motion uses a high-resolution steady clock; animation frames skip native control synchronization. The landscape and logo cache high-quality resamples at their displayed pixel dimensions, rebuilding on scale changes instead of resampling the original image every frame. Buffer presentation uses explicit pixel source/destination bounds to avoid extra DPI scaling.

The Windows client-area animation preference makes transitions immediate and fixes the indeterminate phase. Atmospheric idle motion is authorized by the owner's latest request and limited to foreground home. HTML/CSS snippets in the sidecar are portable visual approximations of these native components; they are not the runtime implementation and cannot prove native semantics or exact GDI+ metrics.

Motion evidence (2026-10-04): `.impeccable/review/motion/` contains deterministic normal/compact time samples from production rendering in a test-owned host. WindowPaintingTests covers interruption, repeated selection, immediate controls, transfer resets, reduced-motion feedback, timer suspension and raster reuse/rebuild across scales. The reduced-motion branch is exercised by injecting the preference; no system preference is changed. A separate visible native-host benchmark measured normal WM_PAINT median intervals improving from 72.63ms to 15.40ms and warm home rendering from 64.32ms to 10.86ms. These are host measurements, not physical display-compositor FPS or a guarantee on every device. The denser GIF preview uses 20ms samples. See `docs/ANIMATIONS.md` for methodology and limits.

### Verification Scope

The ten earlier PNGs in `.impeccable/review/` are deterministic production rendering captures using synthetic view states. They cover ready, loading, Original identity, download, HTTP error, compact, first installation, authentication error, nickname error and sign-in. The representative desktop and compact captures inform the main-page record. Captures omit the native window frame and render nickname text synthetically; they do not exercise the live EDIT, focus, keyboard order, motion, account authentication, network services or game launch.

The earlier main-page independent review disposition was ship, following correction of semantic operation mapping, native dialog tab order and explicit demonstration-news labeling. That keyboard correction was verified in source, not in a new live-window test. `docs/DESIGN_VALIDATION.md` retains the earlier Release build and CTest result (1/1), alongside the limited earlier native-window observations.

The settings review requires `.impeccable/review/settings/settings.png` (1180 × 730), `settings-compact.png` (944 × 584), `settings-error.png` (1180 × 730) and `desktop.png` (1180 × 730). These use the production GDI+ renderer with synthetic values and synthetically drawn EDIT text, omitting the native frame. The error preview source sets initial RAM to 8192 MiB and maximum to 4096 MiB and obtains its message from `LauncherSettingsStore::validate`; it does not fabricate a validation message over valid defaults. These captures show composition and rendering, not live child-control appearance or interaction.

For the earlier settings extension, the implementation handoff reported a Release /W4 build without warnings and CTest 3/3. Those three executables cover existing service regressions, settings JSON round trips and validation, launch arguments and protected paths, plus real EDIT/BUTTON controls in a hidden test-owned native host. The form test covers values, toggles, automatic/custom Java enablement, fullscreen dimension locks, operation locks, invalid input, reset drafts and reopening without saving.

The RAM extension's independent review in `.impeccable/review/ram-slider/review.md` records ship. Its four production-rendered PNGs were opened for the documentation handoff: `settings.png` (1180 × 730), `settings-compact.png` (944 × 584), `settings-half-step.png` (1180 × 730, 6,5 GB / 6656 MiB) and `settings-error.png` (1180 × 730, 8192 initial / 4096 maximum). All show the default minimization checkbox marked. They contain synthetic EDIT/TRACKBAR appearances and omit the native frame. The implementation handoff reports Release /W4 without warnings and CTest 4/4. The added hidden-host painting test covers real TRACKBAR arrow/page-key messages, half-GB/preset selection, exact MiB synchronization and JVM arguments, expanded range, locks, initial-RAM lowering and dark WM_PRINTCLIENT output. Settings tests additionally cover the enabled minimization default, Restore defaults, a preferences file missing that property and a saved explicit false.

Hidden-host tests and synthetic captures do not exercise physical dragging, live focus/tab navigation, caret/selection, desktop cadence, browse dialogs or resize/DPI behavior. No new desktop control was performed after the user stopped the earlier Computer Use session with Escape. Microsoft login, downloads, Minecraft launch, full DPI transitions, reduced-motion preference and animation cadence remain unexercised live; this scoped evidence does not establish runtime guarantees for them.

## Do's and Don'ts

### Do:

- **Do** use the Theme.h palette and the documented semantic state mapping.
- **Do** preserve native window controls, text editing, keyboard order and visible focus.
- **Do** pair status colors with readable labels and concrete operation messages.
- **Do** respect the Windows client-area animation preference and stop the animation timer when activity settles.
- **Do** describe the landscape as concept art and label sample news as demonstration content.
- **Do** verify the native layout and edit control at the intended DPI and window size.

### Don't:

- **Don't** use gameplay connection or player-count claims to decorate HTTP file-service status.
- **Don't** apply Sora to native controls or status copy.
- **Don't** replace the continuous console with repeated elevated cards.
- **Don't** turn an ordinary first installation into an error state.
- **Don't** treat preview renders as evidence of live authentication, downloads or game launch.
- **Don't** infer a mobile layout or browser breakpoint system from the uniformly scaled Windows canvas.

### RAM Selector Verification

The native painting regression now exercises slider notifications at 1, 1.5, 2, 4.5 and 6.5 GB, exact JVM arguments, actual arrow/page-key handling, 4/6/8 GB shortcuts, typed-MiB synchronization, larger-range expansion, operation locks, default reset and dark native WM_PRINTCLIENT output. Release `/W4` and all four CTest targets passed in `out/build/ram-slider`. Production preview renders include normal, compact, invalid initial/maximum and 6.5 GB settings states. These remain synthetic captures; no physical pointer drag or live desktop cadence was measured.
