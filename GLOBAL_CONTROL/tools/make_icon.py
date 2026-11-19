#!/usr/bin/env python3
"""Draws the GLOBAL CONTROL app icon (1024x1024 PNG): graphite squircle, 8 macro arcs around a knob."""
import math, sys
from PIL import Image, ImageDraw, ImageFilter

S = 1024
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.rounded_rectangle([40, 40, S - 40, S - 40], radius=220, fill=(22, 23, 26, 255), outline=(60, 62, 68, 255), width=6)
cx = cy = S // 2
cols = [(232, 163, 61), (63, 193, 217), (214, 93, 177), (143, 209, 79), (240, 124, 58), (91, 141, 239), (229, 83, 75), (62, 201, 167)]
for i, c in enumerate(cols):
    r = 380 - i * 0
    a0 = -90 + i * 45 + 6
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.arc([cx - r, cy - r, cx + r, cy + r], a0, a0 + 33, fill=c + (255,), width=46)
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(14)))
    d.arc([cx - r, cy - r, cx + r, cy + r], a0, a0 + 33, fill=c + (255,), width=34)
d.ellipse([cx - 250, cy - 250, cx + 250, cy + 250], fill=(44, 46, 52, 255), outline=(90, 92, 100, 255), width=8)
d.ellipse([cx - 205, cy - 205, cx + 205, cy + 205], fill=(30, 31, 35, 255))
ang = math.radians(-135 + 270 * 0.68)
x, y = cx + 170 * math.sin(ang), cy - 170 * math.cos(ang)
d.line([cx + 60 * math.sin(ang), cy - 60 * math.cos(ang), x, y], fill=(240, 240, 240, 255), width=26)
img.save(sys.argv[1] if len(sys.argv) > 1 else "icon_1024.png")
