#!/usr/bin/env python3
"""Cuts the 5ynth logo (resources/logo/logo_source.jpg) into transparent PNGs used by the plugin and the manual.
The flat black background of the artwork is keyed out (alpha from the distance to the background colour, colour
un-premultiplied), so the glow of the bars stays soft on any background.
  logo_mark.png   the emblem: metallic "5" with the glowing bars, mini keyboard and power symbol
  logo_word.png   the "5YNTH" wordmark
  logo_full.png   the whole logo (emblem + wordmark)
Run: python3 tools/make_logo.py   (from the SynthZilla folder)"""
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(HERE, "..", "resources", "logo")
src = np.asarray(Image.open(os.path.join(DIR, "logo_source.jpg")).convert("RGB")).astype(np.float32)
bg = np.median(src.reshape(-1, 3), axis=0)

def keyed(y0, y1, x0, x1, pad=14):
    a = src[y0 - pad:y1 + pad + 1, x0 - pad:x1 + pad + 1]
    d = np.abs(a - bg).max(axis=2)
    alpha = np.clip((d - 4.0) / 50.0, 0.0, 1.0)
    safe = np.maximum(alpha, 1e-3)[..., None]
    rgb = np.clip((a - bg * (1.0 - alpha[..., None])) / safe, 0, 255)
    im = Image.fromarray(np.dstack([rgb, alpha * 255.0]).astype(np.uint8), "RGBA")
    return im.crop(im.getbbox())

# rows/columns of the parts in the 1376 x 768 artwork (found by thresholding against the background)
parts = {"mark": (189, 461, 553, 832), "word": (484, 584, 453, 922), "full": (189, 584, 453, 922)}
for name, box in parts.items():
    im = keyed(*box)
    im.save(os.path.join(DIR, f"logo_{name}.png"), optimize=True)
    print(name, im.size)
