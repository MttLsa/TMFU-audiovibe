#!/usr/bin/env python3
"""
Builds AbletonBridge/Build/GLOBAL_CONTROL_BRIDGE.amxd (Max MIDI Effect) from the patch
AbletonBridge/MaxPatch/GLOBAL_CONTROL_BRIDGE.maxpat and copies gc_bridge.js next to it.

.amxd container (Max for Live device, unfrozen):
    'ampf' <u32le 4> <device type: 'mmmm' MIDI effect | 'aaaa' audio effect | 'iiii' instrument>
    'meta' <u32le 4> <u32le 0>
    'ptch' <u32le n> <patcher JSON + '\\n\\0'>
[VERIFICA] Container layout taken from existing unfrozen devices; if Live refuses the file, build the
device by hand (Documentation/TEST_PLAN.md, section 0) and commit it to AbletonBridge/Build/.
The device is NOT frozen: gc_bridge.js must stay in the same folder as the .amxd (the installer copies both).
"""
import json, os, shutil, struct, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = os.path.join(root, "AbletonBridge", "MaxPatch", "GLOBAL_CONTROL_BRIDGE.maxpat")
js = os.path.join(root, "AbletonBridge", "LiveAPI", "gc_bridge.js")
out = os.path.join(root, "AbletonBridge", "Build")

patch = json.load(open(src))
p = patch["patcher"]
p["project"] = {"version": 1, "creationdate": 3590052493, "modificationdate": 3590052493, "viewrect": [0, 0, 300, 500],
                "autoorganize": 1, "hideprojectwindow": 1, "showdependencies": 1, "autolocalize": 0,
                "contents": {"patchers": {}, "code": {"gc_bridge.js": {"kind": "javascript", "local": 1}}},
                "layout": {}, "searchpath": {}, "detailsvisible": 0, "amxdtype": 1835887981, "readonly": 0,
                "devpathtype": 0, "devpath": ".", "sortmode": 0, "viewmode": 0, "includepackages": 0}
p["isolateaudio"] = 0
body = (json.dumps(patch, indent=1) + "\n").encode("utf-8") + b"\0"
data = b"ampf" + struct.pack("<I", 4) + b"mmmm" + b"meta" + struct.pack("<I", 4) + struct.pack("<I", 0) \
     + b"ptch" + struct.pack("<I", len(body)) + body
os.makedirs(out, exist_ok=True)
open(os.path.join(out, "GLOBAL_CONTROL_BRIDGE.amxd"), "wb").write(data)
shutil.copyfile(js, os.path.join(out, "gc_bridge.js"))
open(os.path.join(out, "README.txt"), "w").write(
    "GLOBAL CONTROL bridge - copy BOTH files into the same folder of the Ableton User Library\n"
    "(default: ~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/).\n"
    "GLOBAL CONTROL.app can do it for you: SETUP > Install bridge in User Library.\n"
    "Then create a MIDI track named GLOBAL CONTROL and drop GLOBAL_CONTROL_BRIDGE on it (one per Live Set).\n")
print("written", os.path.join(out, "GLOBAL_CONTROL_BRIDGE.amxd"), len(data), "bytes")
