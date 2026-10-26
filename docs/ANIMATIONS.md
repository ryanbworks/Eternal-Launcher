# Eternal Launcher animations

Motion is drawn with GDI+ into the same frame buffer as the rest of the interface;
no library was added. Native controls (owner-drawn `BUTTON`s, `EDIT`s and the memory
`TRACKBAR`) never move: they copy their pixels from the frame, so focus, Tab, typing
and clicks keep working during every animation.

## What moves

**Window and navigation**
- Pages fade in and rise 16 logical pixels in 300 ms with a cubic ease-out. On the
  home screen the status tiles and news cards follow with a short stagger; News and
  Help stagger their cards the same way.
- The navigation selection is one pill that glides between the icons (60 ms time
  constant), with an accent bar on the edge of the rail.
- Caption buttons, navigation icons and every button fade their hover in and out.

**Home**
- On startup the landscape opens through the portal in 750 ms; particles drift in
  three layers and the portal reacts to the pointer.
- The update toast drops in from the top edge of the landscape and leaves on its own
  after 12 seconds on screen.
- The Nickname / Conta original selection slides between the two cards; the icon and
  text colors follow how much of the selection covers each card.
- The Play button breathes a soft violet halo while it waits, with an occasional
  sheen every 6.5 s. Hover sends one band of light across it, a glow follows the
  pointer and the play icon leans forward with a short trail. Pressing compresses it
  two pixels inside its own hit area; a click releases a ripple. While downloading,
  a glint runs along the filled part of the button.
- Status dots pulse while something is in progress (checking, preparing, playing),
  and the avatar blinks every few seconds.
- News cards lift four pixels with a soft shadow on hover.

**Installation**
- The overall progress bar carries a moving highlight while work is in progress; the
  active stage has an orbiting marker and finished stages a confirmation pulse. The
  transfer bar eases to the real percentage and restarts on each new transfer.

**Settings and Mods**
- The tab underline glides between General, Advanced and About and stretches to each
  tab's width.
- Switches slide in 200 ms with an ease-in-out; the knob stretches slightly while it
  travels and the track color crossfades.

## When frames are drawn

`WM_TIMER` with `USER_TIMER_MINIMUM` runs only while something is moving: a
transition, a hover fading, an indeterminate operation or the home screen's idle
effects. Idle effects (particles, halo, sheen, blinking) only run while the launcher
is the foreground window; they stop when it loses focus, is minimized or hidden in
the notification area. Everything settles to exact values and the timer stops.

The **Show animations in Windows** preference (`SPI_GETCLIENTAREAANIMATION`) makes all
of this instant: selections, switches, focus and progress still change, without
interpolation or idle effects. `WM_SETTINGCHANGE` reapplies it while running.

## Verification

`WindowPaintingTests` checks, among other things: hover easing and settling, the
navigation selection sliding and settling, the page fade stopping its timer, a
settings switch sliding and stopping, the Play sweep running once, the portal energy
stopping after the pointer leaves, timers pausing while hidden or minimized, the
transfer fill restarting per transfer and reduced motion snapping everything.
`WindowPaintingTests --benchmark-motion` prints drawing times for each page at
1280 × 800 and 1024 × 640.

`EternalLauncher.exe --render-preview <folder>` renders every page and state with the
production renderer (the idle effects frozen at a fixed moment), and
`WindowPaintingTests --capture-installation <folder>` captures live windows with their
native controls.
