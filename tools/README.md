# png_to_sprite.py

Converts PNG pixel art into U8g2-compatible XBM C headers for the ESP32/OLED display.

## Requirements

```bash
pip3 install pillow
```

## Basic usage

```bash
python3 tools/png_to_sprite.py src/sprites/egg/egg.png
```

Outputs `egg.h` alongside the PNG. Include it in your code and draw with:

```cpp
#include "sprites/egg/egg.h"

u8g2.drawXBMP(x, y, EGG_WIDTH, EGG_HEIGHT, egg_f0);
```

## Animated sprite sheets

If your PNG is a horizontal strip of frames (e.g. 4 frames of 32x32 = 128x32 total):

```bash
python3 tools/png_to_sprite.py src/sprites/pet/walk.png --frames 4
```

Each frame becomes its own array (`walk_f0`, `walk_f1`, …) plus a pointer array for easy indexing:

```cpp
u8g2.drawXBMP(x, y, WALK_WIDTH, WALK_HEIGHT, walk_frames[tick % WALK_FRAMES]);
```

## Options

| Flag | Default | Description |
|------|---------|-------------|
| `--frames N` | 1 | Number of frames in a horizontal sprite sheet |
| `--out path/to/file.h` | same dir as PNG | Override output path |
| `--invert` | off | Swap on/off — use if your sprite draws inverted |
| `--threshold 0-255` | 128 | Luminance cutoff; pixels below this are "on" |
| `-v` / `--verbose` | off | Print per-frame byte counts |

## Pixel rules

- **Transparent** (alpha < 128) → off
- **Dark pixels** (luminance < threshold) → on
- **Light pixels** → off
- Use `--invert` to flip this if your art uses white-on-black

## Making sprites in Piskel

1. Set canvas to your target size (32x32 recommended for this display)
2. Draw in black on a transparent background
3. **Export → PNG** (not C file — that format is incompatible)
4. Run this script on the exported PNG

## Tips

- 32x32 is a good size for the main character on a 128x64 display
- Keep outlines 2px thick — thin lines can disappear on OLED
- Transparent pixels are always off, so no need to fill the background
