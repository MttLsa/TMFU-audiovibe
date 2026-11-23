#!/usr/bin/env python3
"""
Builds THIRD_PARTY_NOTICES.txt: the licence texts of the third-party components compiled into / shipped with DUALBOX.
The texts are read from the sources used by the build, so they always match the shipped versions.
Usage:  python3 tools/make_notices.py <juce source dir> <output file>
        (the JUCE sources are in build/_deps/juce-src after cmake has configured the build)
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
juce = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "_deps", "juce-src")
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "THIRD_PARTY_NOTICES.txt")
M = os.path.join(juce, "modules")

def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read().strip()

def header_comment(path, max_lines=60):
    """licence text at the top of a source file (first comment block)"""
    lines = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, l in enumerate(f):
            if i >= max_lines: break
            lines.append(l.rstrip())
    return "\n".join(lines).strip()

# (title, description, licence text)
parts = [
    ("Signalsmith Stretch 1.3.2", "Polyphonic pitch / time stretching (Belt pitch shift). https://github.com/Signalsmith-Audio/signalsmith-stretch",
     read(os.path.join(ROOT, "third_party", "signalsmith-stretch", "LICENSE.txt"))),
    ("Signalsmith Linear 0.6.6", "FFT / STFT used by Signalsmith Stretch. https://github.com/Signalsmith-Audio/linear",
     read(os.path.join(ROOT, "third_party", "signalsmith-linear", "LICENSE.txt"))),
    ("Inter typeface", "User-interface font (Inter Regular / SemiBold). https://github.com/rsms/inter",
     read(os.path.join(ROOT, "resources", "fonts", "OFL.txt"))),
    ("JUCE framework", "Application / plug-in framework. Copyright (c) Raw Material Software Limited. https://juce.com",
     read(os.path.join(juce, "LICENSE.md"))),
    ("VST 3 SDK", "VST is a registered trademark of Steinberg Media Technologies GmbH.",
     read(os.path.join(M, "juce_audio_processors", "format_types", "VST3_SDK", "LICENSE.txt"))),
    ("Audio Unit SDK (Apple)", "Audio Unit plug-in support (macOS).",
     read(os.path.join(M, "juce_audio_plugin_client", "AU", "AudioUnitSDK", "LICENSE.txt"))),
    ("FLAC", "FLAC audio file decoding.",
     read(os.path.join(M, "juce_audio_formats", "codecs", "flac", "Flac Licence.txt"))),
    ("Ogg Vorbis", "Ogg Vorbis audio file decoding.",
     read(os.path.join(M, "juce_audio_formats", "codecs", "oggvorbis", "Ogg Vorbis Licence.txt"))),
    ("zlib", "Data compression (factory sample archive).",
     read(os.path.join(M, "juce_core", "zip", "zlib", "README"))),
    ("libpng", "PNG image support.",
     read(os.path.join(M, "juce_graphics", "image_formats", "pnglib", "LICENSE"))),
    ("Independent JPEG Group (jpeglib)", "This software is based in part on the work of the Independent JPEG Group.",
     read(os.path.join(M, "juce_graphics", "image_formats", "jpglib", "README"))),
    ("HarfBuzz", "Text shaping.",
     read(os.path.join(M, "juce_graphics", "fonts", "harfbuzz", "COPYING"))),
    ("SheenBidi", "Bidirectional text support.",
     read(os.path.join(M, "juce_graphics", "unicode", "sheenbidi", "LICENSE"))),
    ("Factory samples", "Credits and licences of the factory sample library.",
     read(os.path.join(ROOT, "resources", "SAMPLE_CREDITS.md"))),
]

sep = "=" * 100
with open(out, "w", encoding="utf-8") as f:
    f.write("DUALBOX - third-party notices\n")
    f.write("DUALBOX is a product of TMFU audiovibe. It contains or uses the third-party components listed below.\n")
    f.write("Each component is the property of its respective authors and is used under the licence reproduced here.\n\n")
    f.write("Contents:\n")
    for t, _, _ in parts: f.write("  - " + t + "\n")
    for t, d, lic in parts:
        f.write("\n" + sep + "\n" + t + "\n" + d + "\n" + sep + "\n\n" + lic + "\n")
print("written", out, os.path.getsize(out), "bytes")
