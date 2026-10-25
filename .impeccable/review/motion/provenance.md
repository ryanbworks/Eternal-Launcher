# Motion preview provenance

Generated on 2026-10-04 by `WindowPaintingTests --capture-motion` using the production
MainPage/Hero/PlayerProfile/InstallationPanel GDI+ rendering in a test-owned native
host. Normal is 1180 × 730; compact is 944 × 584. Portal and identity values are
deterministic time samples, installation states are synthetic, and Steve is
synthetic text added to the home buffer because the native EDIT paints separately.
Settings sequences are native parent WM_PRINTCLIENT and individually mapped child WM_PRINT captures, including real EDITs/BUTTONs
following the transition. Their values and instance path are synthetic. The host
uses the launcher's per-monitor-v2 DPI awareness; pixel presentation is explicit.
Capture flushes invalidated native controls and checks header pixels against the
current production buffer. This avoids stale compositor images returned by
PrintWindow after resizing in this environment.
The settings frames were refreshed during the navigation-performance follow-up
to correct the non-client child-origin offset of the older WM_PRINT/PRF_CHILDREN
fixture. Production appearance and deterministic motion positions are unchanged.
No account credentials, production downloads or Minecraft execution are pictured.
`states/` is the ordinary production `--render-preview` state collection.

The animated GIF is assembled from those unmodified PNG frames using Pillow.
The updated GIF uses 167 production frames, with 20ms samples during movement and
holds on completed states. Its timing illustrates the intended motion and is not
a desktop frame-rate recording. The separate visible-host benchmark reports actual
WM_PAINT intervals and warm drawing times; methodology is in docs/ANIMATIONS.md.

The expanded preview simulates landscape hover, Play hover/press and gear hover;
it also shows a simulated completed Java stage and a Minecraft transfer. It uses
the production rendering parameters and a test-owned BUTTON pressed state, without
activating installation or accessing the network.
The new sequences also sample settings entry/return, 64 landscape particles in
distinct layers, and the cursor-following button light. Input and animation time
are simulated, not a desktop interaction recording.
