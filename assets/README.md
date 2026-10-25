# Eternal visual identity

- `eternal-logo.svg`: custom vector monogram. The stepped E ends in a portal opening. The same geometry is drawn by `DrawingUtils::drawLogo`.
- `eternal.ico`: application icon at 16, 24, 32, 48, 64, 128 and 256 pixels.
- `eternal-icon.png`: brand version for previews.
- `lavender-cosmic-portal.png`: refined Aurora emblem with a transparent background; embedded in the launcher header as a Win32 resource.
- `realm.png`: concept art of a blocky world, produced by the integrated ImageGen tool. It is not a screenshot of the server. The full prompt is in `realm.prompt.txt` and in the PNG metadata.
- `fonts/Sora-Bold.ttf`: typeface for titles and the brand, from [Sora](https://github.com/sora-xor/sora-font/blob/master/fonts/ttf/v2.1beta/Sora-Bold.ttf). SIL OFL license in `fonts/OFL.txt`. Controls and editing keep using Segoe UI.

The landscape, icon, font and its license are embedded as Win32 resources: the distributed launcher doesn't depend on image files next to the executable. The fonts are private to the process and are not installed in Windows.

Brand color: #A582FF. Background: #0D0E14. The SVG is the reusable geometric source; native drawing uses the same coordinates.
