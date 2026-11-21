#!/usr/bin/env python3
"""Cuts the 5ynth logo (resources/logo/logo_source.jpg) into transparent PNGs used by the plugin and the manual.
The flat dark background of the artwork is keyed out (alpha from the distance to the background colour, colour un-premultiplied).
  logo_stack.png    the five stacked keyboards
  logo_word.png      the "5ynth" wordmark
  logo_tagline.png  "5-IN-1 SYNTHESIZER"
  logo_full.png      the whole logo (stack + wordmark + tagline)
Run: python3 tools/make_logo.py   (from the SynthZilla folder)"""
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(HERE, "..", "resources", "logo")
src = np.asarray(Image.open(os.path.join(DIR, "logo_source.jpg")).convert("RGB")).astype(np.float32)
bg = np.median(src.reshape(-1, 3), axis=0)

def keyed(y0, y1, x0, x1, solid=False, pad=6):
    a = src[y0 - pad:y1 + pad + 1, x0 - pad:x1 + pad + 1]
    d = np.abs(a - bg).max(axis=2)
    alpha = np.clip((d - 7.0) / 30.0, 0.0, 1.0)
    if solid:   # keyboards: opaque between the outer edges of each row (dark panels are close to the background colour)
        for y in range(alpha.shape[0]):
            xs = np.where(d[y] > 14.0)[0]
            if len(xs) and xs[-1] - xs[0] > 0.5 * alpha.shape[1]:
                alpha[y, xs[0] + 2:xs[-1] - 1] = 1.0
    safe = np.maximum(alpha, 1e-3)[..., None]
    rgb = np.clip((a - bg * (1.0 - alpha[..., None])) / safe, 0, 255)
    out = np.dstack([rgb, alpha * 255.0]).astype(np.uint8)
    im = Image.fromarray(out, "RGBA")
    return im.crop(im.getbbox())

# rows/columns of the three parts in the 1376 x 768 artwork (found by thresholding against the background)
parts = {"stack": (125, 516, 548, 827), "word": (537, 633, 544, 828), "tagline": (642, 660, 560, 814)}
ims = {}
for name, box in parts.items():
    ims[name] = im = keyed(*box, solid=(name == "stack"))
    im.save(os.path.join(DIR, f"logo_{name}.png"), optimize=True)
    print(name, im.size)
# whole logo, same proportions and spacing as the artwork
logo = Image.new("RGBA", (290, 540), (0, 0, 0, 0))
for name, box in parts.items():
    im = ims[name]
    logo.alpha_composite(im, ((290 - im.width) // 2, box[0] - 125))
logo = logo.crop(logo.getbbox())
logo.save(os.path.join(DIR, "logo_full.png"), optimize=True)
print("logo", logo.size)
