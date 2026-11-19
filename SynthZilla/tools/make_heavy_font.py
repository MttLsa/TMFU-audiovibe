"""Heavier bold for the UI: Comfortaa Bold (the heaviest Comfortaa weight) with thicker strokes.
SIL OFL 1.1: a modified version must not use the Reserved Font Name "Comfortaa" -> it is named "SynthZilla Heavy".
Needs FontForge's Python module:  python3 tools/make_heavy_font.py   (Debian/Ubuntu: apt install python3-fontforge)
"""
import os, fontforge

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, '..', 'resources', 'fonts')
EXTRA = 26                                     # extra stroke width in font units (UPM 1000)

f = fontforge.open(os.path.join(FONTS, 'Comfortaa-Bold.ttf'))
f.is_quadratic = False                         # work on cubic outlines (changeWeight needs them)
f.selection.all()
f.changeWeight(EXTRA, 'LCG', 0, 0, 'squish')   # embolden inside the same advance widths (labels keep their length)
f.removeOverlap(); f.round(); f.correctDirection()
f.fontname, f.familyname, f.fullname = 'SynthZillaHeavy-Regular', 'SynthZilla Heavy', 'SynthZilla Heavy'
f.os2_weight = 800
f.appendSFNTName('English (US)', 'Copyright', 'Copyright 2011 The Comfortaa Project Authors (https://github.com/alexeiva/comfortaa). '
                 'Modified (heavier strokes) for SynthZilla. Licensed under the SIL Open Font License 1.1.')
f.generate(os.path.join(FONTS, 'SynthZillaHeavy.ttf'))
print('ok')
