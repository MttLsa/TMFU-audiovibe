"""SynthZilla logo: the kaiju artwork (resources/logo/kaiju_source.png, unchanged) standing on a synth keyboard.
Writes resources/logo/kaiju_keyboard.png (embedded in the plug-in) and a large preview in docs/.
    python3 tools/make_logo.py
"""
import os
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'resources', 'logo', 'kaiju_source.png')
OUT = os.path.join(HERE, '..', 'resources', 'logo', 'kaiju_keyboard.png')

SS = 4                                   # supersampling
KW, KH = 440, 92                         # keyboard size in source pixels
ANGLE = 0                                # degrees (counter-clockwise in PIL)
CENTRE = (350, 656)                      # keyboard centre: under the feet (they rest on its top edge)
INK = (17, 17, 20, 255)
BODY, BODY_HI, PANEL = (64, 60, 78, 255), (98, 93, 118, 255), (44, 41, 54, 255)
WHITE, KEYSHADE = (250, 250, 252, 255), (196, 204, 214, 255)

def keyboard():
    w, h = KW * SS, KH * SS
    pad = 6 * SS
    im = Image.new('RGBA', (w + 2 * pad, h + 2 * pad), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    o = 4 * SS                           # outline width, like the artwork
    x0, y0, x1, y1 = pad, pad, pad + w, pad + h
    d.rounded_rectangle((x0, y0, x1, y1), radius=7 * SS, fill=INK)
    d.rounded_rectangle((x0 + o, y0 + o, x1 - o, y1 - o), radius=5 * SS, fill=BODY)
    d.rounded_rectangle((x0 + o, y0 + o, x1 - o, y0 + o + 5 * SS), radius=4 * SS, fill=BODY_HI)   # top highlight
    # control panel: screen, knobs, LED
    py0, py1 = y0 + o + 4 * SS, y0 + 24 * SS
    d.rounded_rectangle((x0 + 12 * SS, py0, x0 + 46 * SS, py1), radius=2 * SS, fill=INK)
    d.rectangle((x0 + 15 * SS, py0 + 3 * SS, x0 + 43 * SS, py1 - 3 * SS), fill=(160, 214, 60, 255))
    for i in range(5):
        cx, cy, r = x0 + (60 + i * 17) * SS, (py0 + py1) // 2, 5 * SS
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=INK)
        d.ellipse((cx - r + 2 * SS, cy - r + 2 * SS, cx + r - 2 * SS, cy + r - 2 * SS), fill=(230, 230, 236, 255))
        d.line((cx, cy, cx + int(r * 0.5), cy - int(r * 0.7)), fill=INK, width=2 * SS)
    lx, ly, lr = x1 - 22 * SS, (py0 + py1) // 2, 4 * SS
    d.ellipse((lx - lr - SS, ly - lr - SS, lx + lr + SS, ly + lr + SS), fill=INK)
    d.ellipse((lx - lr, ly - lr, lx + lr, ly + lr), fill=(226, 52, 52, 255))
    d.ellipse((lx - lr // 2, ly - lr // 2 - SS, lx, ly - SS), fill=(255, 170, 170, 255))
    # keys
    kx0, kx1, ky0, ky1 = x0 + 9 * SS, x1 - 9 * SS, y0 + 27 * SS, y1 - 7 * SS
    d.rectangle((kx0 - 2 * SS, ky0 - 2 * SS, kx1 + 2 * SS, ky1 + 2 * SS), fill=INK)
    n = 15
    kw = (kx1 - kx0) / n
    for k in range(n):
        a, b = int(kx0 + k * kw + SS), int(kx0 + (k + 1) * kw - SS)
        d.rectangle((a, ky0, b, ky1), fill=WHITE)
        d.rectangle((a, ky1 - 4 * SS, b, ky1), fill=KEYSHADE)
    for k in range(n - 1):
        if k % 7 in (2, 6):              # E-F and B-C: no black key
            continue
        cx = kx0 + (k + 1) * kw
        bw = kw * 0.62
        d.rounded_rectangle((int(cx - bw / 2), ky0, int(cx + bw / 2), int(ky0 + (ky1 - ky0) * 0.6)), radius=SS, fill=INK)
    im = im.rotate(ANGLE, resample=Image.BICUBIC, expand=True)
    return im.resize((im.width // SS, im.height // SS), Image.LANCZOS)

def main():
    src = Image.open(SRC).convert('RGBA')
    kb = keyboard()
    canvas = Image.new('RGBA', (src.width, CENTRE[1] + kb.height // 2 + 20), (0, 0, 0, 0))
    canvas.alpha_composite(kb, (CENTRE[0] - kb.width // 2, CENTRE[1] - kb.height // 2))
    canvas.alpha_composite(src, (0, 0))          # the kaiju exactly as drawn, in front: its feet stand on the keyboard
    out = canvas
    # white sticker border (the artwork sits on white): keeps the grey kaiju readable on the dark UI
    a = out.split()[3].point(lambda v: 255 if v > 40 else 0)
    border = a.filter(ImageFilter.MaxFilter(15)).filter(ImageFilter.GaussianBlur(1.5))
    sticker = Image.new('RGBA', out.size, (255, 255, 255, 0))
    sticker.putalpha(border)
    sticker.alpha_composite(out)
    out = sticker.crop(sticker.getbbox())
    out.save(OUT, optimize=True)
    print('wrote', OUT, out.size)

if __name__ == '__main__':
    main()
