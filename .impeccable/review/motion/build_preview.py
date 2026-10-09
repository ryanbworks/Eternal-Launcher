"""Encode deterministic production time samples; no frame resizing or retouching."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
for variant in ("normal", "compact"):
    names = [f"portal-{i:02d}" for i in range(39)]
    names += [f"interaction-{i}" for i in range(41)]
    names += [f"settings-in-{i}" for i in range(18)]
    names += [f"settings-out-{i}" for i in range(15)]
    names += [f"identity-{i}" for i in range(19)]
    names += [f"installation-{i}" for i in range(13)]
    names += [f"completion-{i}" for i in range(22)]
    frames = [Image.open(root / f"{name}-{variant}.png").convert("RGB") for name in names]
    palette = frames[38].quantize(colors=256)
    encoded = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    # GIF uses 10ms units. Consistent 20ms samples avoid undersampled short transitions.
    durations = ([20] * 38 + [300] + [20] * 40 + [300] + [20] * 17 + [700] + [20] * 14 + [300]
                 + [20] * 18 + [300] + [20] * 12 + [300] + [20] * 21 + [1000])
    output = root / f"eternal-motion-{variant}.gif"
    encoded[0].save(output, save_all=True, append_images=encoded[1:], duration=durations,
                    loop=0, disposal=2, optimize=False)
    print(f"{output.name}: {len(encoded)} production frames, {output.stat().st_size} bytes")
