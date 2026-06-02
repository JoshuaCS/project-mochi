#!/usr/bin/env python3
"""Generates a 128×64 pixel-art kitty PNG for import into Piskel.
The cat sprite is ~30 px tall, centred on the canvas.
Run: python3 tools/gen_kitty.py
Requires: pip install Pillow"""

from PIL import Image, ImageDraw

W, H = 128, 64
img = Image.new("RGB", (W, H), (255, 255, 255))
d = ImageDraw.Draw(img)
K  = (0, 0, 0)
WH = (255, 255, 255)

cx, cy = 56, 25  # head centre; offset left to leave room for tail

# --- Ears (drawn first so head outline overlaps the base) ---
d.polygon([(cx-7, cy-4), (cx-11, cy-8), (cx-2, cy-6)], outline=K, fill=WH)
d.polygon([(cx+7, cy-4), (cx+11, cy-8), (cx+2, cy-6)], outline=K, fill=WH)
d.polygon([(cx-7, cy-5), (cx-10, cy-7), (cx-3, cy-6)], fill=K)  # inner left
d.polygon([(cx+7, cy-5), (cx+10, cy-7), (cx+3, cy-6)], fill=K)  # inner right

# --- Head ---
d.ellipse([cx-7, cy-7, cx+7, cy+7], outline=K, fill=WH)

# --- Eyes ---
d.ellipse([cx-5, cy-2, cx-2, cy+1], fill=K)
d.ellipse([cx+2, cy-2, cx+5, cy+1], fill=K)
d.point([cx-4, cy-1], fill=WH)  # shine
d.point([cx+3, cy-1], fill=WH)

# --- Nose ---
d.polygon([(cx, cy+3), (cx-1, cy+2), (cx+1, cy+2)], fill=K)

# --- Mouth ---
d.line([(cx, cy+3), (cx-2, cy+5)], fill=K)
d.line([(cx, cy+3), (cx+2, cy+5)], fill=K)

# --- Whiskers ---
d.line([(cx-2, cy+1), (cx-12, cy)],   fill=K)
d.line([(cx-2, cy+3), (cx-12, cy+4)], fill=K)
d.line([(cx+2, cy+1), (cx+12, cy)],   fill=K)
d.line([(cx+2, cy+3), (cx+12, cy+4)], fill=K)

# --- Body ---
d.ellipse([cx-7, cy+7, cx+7, cy+19], outline=K, fill=WH)

# --- Paws ---
d.ellipse([cx-7, cy+17, cx-2, cy+22], outline=K, fill=WH)
d.ellipse([cx+2, cy+17, cx+7, cy+22], outline=K, fill=WH)

# --- Tail (line segments approximating an S-curve to the right) ---
tail = [
    (cx+7,  cy+15),
    (cx+13, cy+18),
    (cx+18, cy+15),
    (cx+20, cy+9),
    (cx+17, cy+4),
]
for i in range(len(tail) - 1):
    d.line([tail[i], tail[i+1]], fill=K)

img.save("kitty.png")
print("Saved kitty.png — 128×64 canvas, kitty ~30 px tall")
print("Sprite spans x=44–76, y=17–47")
